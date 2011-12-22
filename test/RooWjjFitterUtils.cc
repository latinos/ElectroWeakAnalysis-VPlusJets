#include "RooWjjFitterUtils.h"

#include "TFile.h"
#include "TH1D.h"
#include "TTree.h"
#include "TLegend.h"
#include "TEventList.h"
#include "TRandom3.h"
#include "TMath.h"

#ifndef __CINT__
#include "RooGlobalFunc.h"
#endif
#include "RooWorkspace.h"
#include "RooRealVar.h"
#include "RooDataHist.h"
#include "RooHistPdf.h"
#include "RooArgSet.h"
#include "RooArgList.h"
#include "RooFormulaVar.h"
#include "RooDataSet.h"
#include "RooPlot.h"
#include "RooBinning.h"

RooWjjFitterUtils::RooWjjFitterUtils()
{
  initialize();
}

RooWjjFitterUtils::RooWjjFitterUtils(RooWjjFitterParams & pars) :
  params_(pars), binArray(0),
  effEleReco((params_.effDir + "eleEffsSCToReco_ScaleFactors.txt").Data()),
  effEleId((params_.effDir + "eleEffsRecoToWP80_ScaleFactors.txt").Data()),
  effEle((params_.effDir + "eleEffsWP80ToHLTEle27_May10ReReco.txt").Data()),
  effEle2((params_.effDir + "eleEffsHLTEle2jPfMht_data_LWA_Ele.txt").Data()),
  effMuId((params_.effDir + "muonEffsRecoToIso_ScaleFactors.txt").Data()),
  effMu((params_.effDir + "muonEffsIsoToHLT_data_LP_LWA.txt").Data()),
  effJ30((params_.effDir + "eleEffsHLTEle2jPfMht_data_LWA_Jet30.txt").Data()),
  effJ25NoJ30((params_.effDir + "eleEffsHLTEle2jPfMht_data_LWA_Jet25Not30.txt").Data()),
  effMHT((params_.effDir + "eleEffsHLTEle2jPfMht_data_LWA_PfMht.txt").Data())
{
  initialize();
}

RooWjjFitterUtils::~RooWjjFitterUtils()  { 
  delete mjj_; 
  delete massVar_;
  delete binArray;
}

void RooWjjFitterUtils::vars2ws(RooWorkspace& ws) const {
  ws.import(*massVar_, RooFit::RecycleConflictNodes(), RooFit::Silence());
}

void RooWjjFitterUtils::initialize() {
  updatenjets();
  mjj_ = new RooRealVar(params_.var, "m_{jj}", params_.minMass, 
			params_.maxMass, "GeV");
  mjj_->setPlotLabel(mjj_->GetTitle());
  mjj_->setBins(params_.nbins);
  massVar_ = new RooFormulaVar("mass", "@0", RooArgList( *mjj_));  

  if (params_.binEdges.size() > 1) {
    binArray = new double[params_.binEdges.size()];
    for (unsigned int i = 0; i<params_.binEdges.size(); ++i)
      binArray[i] = params_.binEdges[i];
    RooBinning varBins(params_.binEdges.size()-1, binArray, "plotBinning");
    varBins.Print("v");
    mjj_->setBinning(varBins, "plotBinning");
    mjj_->setBinning(varBins);
  } else {
    RooBinning evenBins(params_.nbins, params_.minMass, params_.maxMass); 
    mjj_->setBinning(evenBins, "plotBinning");
    mjj_->setBinning(evenBins);  
  }
}

TH1 * RooWjjFitterUtils::newEmptyHist(TString histName, double binMult) const {
  TH1D * theHist;
  if (binArray)
    theHist = new TH1D(histName, histName, 
		       params_.binEdges.size()-1, binArray);
  else
    theHist = new TH1D(histName, histName, int(params_.nbins*binMult), 
		       params_.minMass, params_.maxMass);
  theHist->Sumw2();
  return theHist;
}

TH1 * RooWjjFitterUtils::File2Hist(TString fname, 
				   TString histName, bool isElectron,
				   int jes_scl, bool noCuts, 
				   double binMult, TString cutOverride) const {
  TFile * treeFile = TFile::Open(fname);
  TTree * theTree;
  treeFile->GetObject(params_.treeName, theTree);
  if (!theTree) {
    std::cout << "failed to find tree " << params_.treeName << " in file " << fname 
	      << '\n';
    return 0;
  }

  double tmpScale = 0.;
  if ((jes_scl >= 0) && (jes_scl < int(params_.JES_scales.size())))
    tmpScale = params_.JES_scales[jes_scl];
//   TString plotStr = TString::Format("%s*(1+%0.4f)", params_.var.Data(), 
// 				    tmpScale);
  TH1 * theHist = newEmptyHist(histName, binMult);

  TString theCuts(cutOverride);
  if (theCuts.Length() < 1)
    theCuts = fullCuts();
  theTree->Draw(">>" + histName + "_evtList", 
		((noCuts) ? TString("") : theCuts),
		"goff");
  TEventList * list = (TEventList *)gDirectory->Get(histName + "_evtList");

  bool localDoWeights = params_.doEffCorrections && (!noCuts) && 
    (cutOverride.Length() < 1);

  static unsigned int const maxJets = 6;
  activateBranches(*theTree, isElectron);
  Float_t         JetPFCor_Pt[maxJets];
  Float_t         JetPFCor_Eta[maxJets];
  Float_t         Mass2j_PFCor;
  Float_t         event_met_pfmet;
  Int_t           event_nPV;
  Int_t           evtNJ;
  Float_t         lepton_pt;
  Float_t         lepton_eta;

  theTree->SetBranchAddress(params_.var, &Mass2j_PFCor);
  if (localDoWeights) {
    theTree->SetBranchAddress("JetPFCor_Pt",JetPFCor_Pt);
    theTree->SetBranchAddress("JetPFCor_Eta",JetPFCor_Eta);
    theTree->SetBranchAddress("event_nPV", &event_nPV);
    theTree->SetBranchAddress("event_met_pfmet", &event_met_pfmet);
    theTree->SetBranchAddress("evtNJ", &evtNJ);
    if(isElectron) {
      theTree->SetBranchAddress("W_electron_pt", &lepton_pt);
      theTree->SetBranchAddress("W_electron_eta", &lepton_eta);
    } else {
      theTree->SetBranchAddress("W_muon_pt", &lepton_pt);
      theTree->SetBranchAddress("W_muon_eta", &lepton_eta);
    }
  }

  static double const singleElectronCutOffLumi = 1000.;
  double evtWgt = 1.0, hltEffJets = 1.0, hltEffMHT = 1.0;
  std::vector<double> eff30(maxJets), eff25n30(maxJets);
  static TRandom3 rnd(987654321);
  for (int event = 0; event < list->GetN(); ++event) {
    theTree->GetEntry(list->GetEntry(event));
    evtWgt = 1.0;
    if (localDoWeights) {
      if (isElectron) {
	evtWgt *= effEleReco.GetEfficiency(lepton_pt, lepton_eta);
	evtWgt *= effEleId.GetEfficiency(lepton_pt, lepton_eta);
	evtWgt *= effEle.GetEfficiency(lepton_pt, lepton_eta);
      } else {
	evtWgt *= effMuId.GetEfficiency(lepton_pt, lepton_eta);
	evtWgt *= effMu.GetEfficiency(lepton_pt, lepton_eta);
      }

      for (unsigned int i = 0; i < maxJets; ++i) {
	eff30[i] = effJ30.GetEfficiency(JetPFCor_Pt[i], JetPFCor_Eta[i]);
	eff25n30[i] = effJ25NoJ30.GetEfficiency(JetPFCor_Pt[i], 
						JetPFCor_Eta[i]);
      }
      hltEffJets = dijetEff(evtNJ, eff30, eff25n30);
      hltEffMHT = effMHT.GetEfficiency(event_met_pfmet, 0.);

      if ( (isElectron) && 
	   (rnd.Rndm() > singleElectronCutOffLumi/params_.intLumi) )
	evtWgt *= hltEffJets*hltEffMHT;
    }
    
    theHist->Fill(Mass2j_PFCor*(1.+tmpScale), evtWgt);
  }

  delete theTree;

  theHist->SetDirectory(0);

  delete treeFile;

  //theHist->Print();
  return theHist;
}

RooAbsPdf * RooWjjFitterUtils::Hist2Pdf(TH1 * hist, TString pdfName,
					 RooWorkspace& ws) const {
  if (ws.pdf(pdfName))
    return ws.pdf(pdfName);

  RooDataHist newHist(pdfName + "_hist", pdfName + "_hist",
		      RooArgList(*mjj_), hist);
  ws.import(newHist);

  RooHistPdf thePdf = RooHistPdf(pdfName, pdfName, RooArgSet(*massVar_),
				 RooArgSet(*mjj_), newHist);
  //thePdf.Print();
  ws.import(thePdf, RooFit::RecycleConflictNodes(), RooFit::Silence());

  return ws.pdf(pdfName);
}

RooDataSet * RooWjjFitterUtils::File2Dataset(TString fname, 
					     TString dsName, bool isElectron,
					     bool trunc, bool noCuts) const {
  TFile * treeFile = TFile::Open(fname);
  TTree * theTree;
  treeFile->GetObject(params_.treeName, theTree);
  if (!theTree) {
    std::cout << "failed to find tree " << params_.treeName << " in file " << fname 
	      << '\n';
    return 0;
  }

  TTree * reducedTree = theTree;

  TFile holder("holder_DELETE_ME.root", "recreate");
  if (!noCuts) {
    activateBranches(*theTree, isElectron);
    reducedTree = theTree->CopyTree( fullCuts(trunc) );
    delete theTree;
  }

  //reducedTree->Print();
  RooDataSet * ds = new RooDataSet(dsName, dsName, reducedTree, 
				   RooArgSet(*mjj_));

  delete reducedTree;
  delete treeFile;

  return ds;
}

TString RooWjjFitterUtils::fullCuts(bool trunc) const {
  TString theCut = TString::Format("((%s > %0.3f) && (%s < %0.3f))", 
				    params_.var.Data(), params_.minMass, 
				    params_.var.Data(), params_.maxMass);
  if (trunc) {
    theCut = TString::Format("(((%s>%0.3f) && (%s<%0.3f)) || "
			     "((%s>%0.3f) && (%s<%0.3f)))",
			     params_.var.Data(), params_.minMass,
			     params_.var.Data(), params_.minTrunc,
			     params_.var.Data(), params_.maxTrunc,
			     params_.var.Data(), params_.maxMass);
  }
  theCut += TString::Format(" && (%s)", jetCut_.Data());
  if (params_.cuts.Length() > 0)
    theCut +=  TString::Format(" && (%s)", params_.cuts.Data());
  return "(" + theCut + ")";
}

void RooWjjFitterUtils::hist2RandomTree(TH1 * theHist, 
					TString fname) const {
  TFile treeFile(fname, "recreate");
  TTree WJet(params_.treeName, params_.treeName);
  double theVar;
  WJet.Branch(params_.var, &theVar, params_.var + "/D");
  double entries = 0;
  for (entries = 0; entries < theHist->GetEntries(); ++entries) {
    theVar = theHist->GetRandom();
    WJet.Fill();
  }
  WJet.Write();
  treeFile.Close();
}

void RooWjjFitterUtils::updatenjets() {
  jetCut_ = TString::Format("evtNJ==%d", params_.njets);
  if (params_.njets < 2)
    jetCut_ = "evtNJ==2 || evtNJ==3";
}

double RooWjjFitterUtils::computeChi2(RooHist& hist, RooAbsPdf& pdf, 
				      RooRealVar& obs, int& nbin) {
  int np = hist.GetN();
  nbin = 0;
  double chi2(0);

  double x,y,eyl,eyh,exl,exh;
  double avg, pdfSig2;
  double Npdf = pdf.expectedEvents(RooArgSet(obs));
  TString className;
  RooAbsReal * binInt;
  RooAbsReal * fullInt = pdf.createIntegral(obs, RooFit::NormSet(obs));
  for (int i=0; i<np; ++i) {
    hist.GetPoint(i,x,y);
    eyl = hist.GetEYlow()[i];
    eyh = hist.GetEYhigh()[i];
    exl = hist.GetEXlow()[i];
    exh = hist.GetEXhigh()[i];

    obs.setVal(x);
    obs.setRange("binRange", x-exl, x+exh);
    binInt = pdf.createIntegral(obs, RooFit::NormSet(obs),
				RooFit::Range("binRange"));
    avg = Npdf*binInt->getVal()/fullInt->getVal();
    delete binInt;

    pdfSig2 = 0.;
    className = pdf.ClassName();
//     std::cout << TString::Format("bin [%0.2f, %0.2f]", x-exl, x+exh) << '\n';
    if (className == "RooHistPdf") {
      RooHistPdf& tmpPdf = dynamic_cast<RooHistPdf&>(pdf);
      pdfSig2 = sig2(tmpPdf, obs, avg);
    } else if (className == "RooAddPdf") {
      RooAddPdf& tmpPdf = dynamic_cast<RooAddPdf&>(pdf);
      pdfSig2 = sig2(tmpPdf, obs, avg);
    }
    
//     std::cout << "y: " << y << " avg: " << avg << '\n';
    if (y != 0) {
      ++nbin;
      double pull2 = (y-avg)*(y-avg);
      pull2 = (y>avg) ? pull2/(eyl*eyl + pdfSig2) : pull2/(eyh*eyh + pdfSig2) ;
      chi2 += pull2;
    }
  }
  delete fullInt;
  return chi2;
}

TLegend * RooWjjFitterUtils::legend4Plot(RooPlot * plot) {
  TObject * theObj;
  TString objName, objTitle;
  TLegend * theLeg = new TLegend(0.70, 0.65, 0.92, 0.92, "", "NDC");
  theLeg->SetName("theLegend");

  theLeg->SetBorderSize(0);
  theLeg->SetLineColor(0);
  theLeg->SetFillColor(0);
  theLeg->SetFillStyle(0);
  theLeg->SetLineWidth(0);
  theLeg->SetLineStyle(0);
  theLeg->SetTextFont(42);
  int entryCnt = 0;
  for(int obj=0; obj < plot->numItems(); ++obj) {
    objName = plot->nameOf(obj);
    if (!(plot->getInvisible(objName))) {
      theObj = plot->getObject(obj);
      objTitle = theObj->GetTitle();
      if (objTitle.Length() < 1)
	objTitle = objName;
      theLeg->AddEntry(theObj, objTitle, plot->getDrawOptions(objName));
      ++entryCnt;
    }
  }
  theLeg->SetY1NDC(0.92 - 0.04*entryCnt - 0.02);
  theLeg->SetY1(theLeg->GetY1NDC());
  return theLeg;
}

void RooWjjFitterUtils::activateBranches(TTree& t, bool isElectron) {
  t.SetBranchStatus("*",    0);
  t.SetBranchStatus("JetPFCor_Pt",    1);
  t.SetBranchStatus("JetPFCor_Px",    1);
  t.SetBranchStatus("JetPFCor_Py",    1);
  t.SetBranchStatus("JetPFCor_Pz",    1);
  t.SetBranchStatus("JetPFCor_Eta",    1);
  t.SetBranchStatus("JetPFCor_Phi",    1);
  t.SetBranchStatus("JetPFCor_bDiscriminator",    1);
  t.SetBranchStatus("JetPFCor_QGLikelihood",    1);

  t.SetBranchStatus("event_met_pfmet",    1);
  t.SetBranchStatus("event_met_pfmetPhi",    1);
  t.SetBranchStatus("event_met_pfmetsignificance",    1);
  t.SetBranchStatus("event_BeamSpot_x",    1);
  t.SetBranchStatus("event_BeamSpot_y",    1);
  t.SetBranchStatus("event_RhoForLeptonIsolation",    1);
  t.SetBranchStatus("event_nPV",    1);

  if (isElectron) {
    t.SetBranchStatus("W_electron_pt", 1);
    t.SetBranchStatus("W_electron_eta", 1);
  } else {
    t.SetBranchStatus("W_muon_pt", 1);
    t.SetBranchStatus("W_muon_eta", 1);
  }

  t.SetBranchStatus("W_mt",    1);
  t.SetBranchStatus("W_pt",    1);
  t.SetBranchStatus("W_pzNu1",    1);
  t.SetBranchStatus("W_pzNu2",    1);
  t.SetBranchStatus("fit_status",    1);
//   t.SetBranchStatus("gdevtt",    1);
  t.SetBranchStatus("fit_chi2",    1);
  t.SetBranchStatus("fit_NDF",    1);
  t.SetBranchStatus("fit_mlvjj", 1);
  t.SetBranchStatus("evtNJ",    1);

  t.SetBranchStatus("Mass2j_PFCor",    1);
  t.SetBranchStatus("MassV2j_PFCor",    1);
}

double RooWjjFitterUtils::dijetEff(int Njets, 
				   std::vector<double> const& eff30,
				   std::vector<double> const& eff25n30) {

//   if (Njets == 2) {
//     return (eff30[0]*eff30[1]+eff30[0]*eff25n30[1]+eff30[1]*eff25n30[0]);
//   }
  int N = TMath::Power(3, Njets), n, t, oneCnt, twoCnt;
  unsigned int i;
  std::vector<int> digits(Njets, 0);
  double theEff(0.), tmpEff(1.0);
  for(n=0; n<N; ++n) {
    t = n;
    oneCnt = 0;
    twoCnt = 0;
    i = digits.size()-1;

    //base 3 conversion
    while (t>0) {
      digits[i] = t%3;
      t /= 3; 
      switch(digits[i--]){
      case 1:
	oneCnt++;
	break;
      case 2:
	twoCnt++;
	break;
      }
    }

    if ((twoCnt > 0) && ((twoCnt > 1) || (oneCnt > 0))) {
      tmpEff = 1.0;
      for (i = 0; i<digits.size(); ++i) {
	switch (digits[i]) {
	case 0:
	  tmpEff *= 1. - eff30[i] - eff25n30[i];
	  break;
	case 1:
	  tmpEff *= eff25n30[i];
	  break;
	case 2:
	  tmpEff *= eff30[i];
	  break;
	}
      }
      theEff += tmpEff;
    }
  }
  return theEff;
}

double RooWjjFitterUtils::sig2(RooAddPdf& pdf, RooRealVar& obs, double Nbin) {

  double retVal = 0.;
  TString className;
  double iN = 0, sumf = 0.;
  bool allCoefs(false);
  bool fCoefs(true);
  if (pdf.coefList().getSize() == pdf.pdfList().getSize())
    allCoefs = true;
  if (pdf.coefList().getSize()+1 == pdf.pdfList().getSize())
    fCoefs = true;
  RooAbsReal * coef = 0;
  RooAbsPdf * ipdf = 0;
//   std::cout << "pdf: " << pdf.GetName() 
// 	    << " N for pdf: " << Nbin
// 	    << '\n';
  for (int i = 0; i < pdf.pdfList().getSize(); ++i) {
    ipdf = dynamic_cast<RooAbsPdf *>(pdf.pdfList().at(i));
    coef = 0;
    iN = 0;
    if (pdf.coefList().getSize() > i)
      coef = dynamic_cast< RooAbsReal * >(pdf.coefList().at(i));
    RooAbsReal * fullInt = ipdf->createIntegral(obs, RooFit::NormSet(obs));
    if ((allCoefs) && (coef)) {
      iN = coef->getVal();
      RooAbsReal * binInt = ipdf->createIntegral(obs, RooFit::NormSet(obs),
						 RooFit::Range("binRange"));
      iN *= binInt->getVal()/fullInt->getVal();
      delete binInt;

    } else if ((fCoefs) && (coef)) {
      iN = coef->getVal()*Nbin;
      sumf += coef->getVal();
    } else if (fCoefs) {
      iN = (1.-sumf)*Nbin;
    } else {
      iN = ipdf->expectedEvents(RooArgSet(obs));
      RooAbsReal * binInt = ipdf->createIntegral(obs, RooFit::NormSet(obs),
						 RooFit::Range("binRange"));
      iN *= binInt->getVal()/fullInt->getVal();
      delete binInt;
    }
    delete fullInt;
    className = ipdf->ClassName();
    if (className == "RooHistPdf") {
      RooHistPdf * tmpPdf = dynamic_cast<RooHistPdf *>(ipdf);
      retVal += sig2(*tmpPdf, obs, iN);
    } else if (className == "RooAddPdf") {
      RooAddPdf * tmpPdf = dynamic_cast<RooAddPdf *>(ipdf);
      retVal += sig2(*tmpPdf, obs, iN);
    }
  }
  return retVal;

}

double RooWjjFitterUtils::sig2(RooHistPdf& pdf, RooRealVar& obs, double Nbin) {

  obs.setVal(obs.getMin("binRange"));
  double binVol, weight, weightErr;
  double sumw = 0., sumw2 = 0.;
//   std::cout << "pdf: " << pdf.GetName()
// 	    << " Nbin: " << Nbin;
  while (obs.getVal() < obs.getMax("binRange")) {
    pdf.dataHist().get(obs);
    binVol = pdf.dataHist().binVolume();
    weight = pdf.dataHist().weight();
    weightErr = pdf.dataHist().weightError(RooAbsData::SumW2);
//     std::cout << " binVol: " << binVol
// 	      << " weight: " << weight
// 	      << " weightErr: " << weightErr
// 	      << ' ';
    sumw += weight;
    sumw2 += weightErr*weightErr;
    obs.setVal(obs.getVal() + binVol);
  }
//   std::cout << " sumw: " << sumw
// 	    << " sumw2: " << sumw2;
  if (sumw == 0) return 0.;
  double retVal = Nbin*sqrt(sumw2)/sumw;
//   std::cout << " sig: " << retVal << '\n';
  return retVal*retVal;

}
