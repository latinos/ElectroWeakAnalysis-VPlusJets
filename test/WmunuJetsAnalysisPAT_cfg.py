import FWCore.ParameterSet.Config as cms
import pprint
isMC = False

process = cms.Process("demo")

##---------  Load standard Reco modules ------------
process.load("FWCore.MessageService.MessageLogger_cfi")
process.load('Configuration.StandardSequences.Services_cff')
process.load('Configuration.StandardSequences.MagneticField_38T_cff')



##----- this config frament brings you the generator information ----
process.load("SimGeneral.HepPDTESSource.pythiapdt_cfi")
process.load("PhysicsTools.HepMCCandAlgos.genParticles_cfi")
process.load("Configuration.StandardSequences.Generator_cff")


##----- Detector geometry : some of these needed for b-tag -------
process.load("TrackingTools.TransientTrack.TransientTrackBuilder_cfi")
process.load("Configuration.StandardSequences.Geometry_cff")
process.load("Geometry.CMSCommonData.cmsIdealGeometryXML_cfi")
process.load("Geometry.CommonDetUnit.globalTrackingGeometry_cfi")
process.load("RecoMuon.DetLayers.muonDetLayerGeometry_cfi")


##----- B-tags --------------
process.load("RecoBTag.Configuration.RecoBTag_cff")


##----- Global tag: conditions database ------------
process.load('Configuration.StandardSequences.FrontierConditions_GlobalTag_cff')

## import skeleton process
#from PhysicsTools.PatAlgos.patTemplate_cfg import *


############################################
if not isMC:
    process.GlobalTag.globaltag = 'GR_R_53_V10::All'
else:
    process.GlobalTag.globaltag = 'START53_V7E::All'

OutputFileName = "WmunuJetAnalysisntuple.root"
numEventsToRun = 1000
############################################
########################################################################################
########################################################################################

##---------  W-->munu Collection ------------
process.load("ElectroWeakAnalysis.VPlusJets.WmunuCollectionsPAT_cfi")

##---------  Jet Collection ----------------
process.load("ElectroWeakAnalysis.VPlusJets.JetCollectionsPAT_cfi")

##---------  Vertex and track Collections -----------
process.load("ElectroWeakAnalysis.VPlusJets.TrackCollections_cfi")
#


process.maxEvents = cms.untracked.PSet(
    input = cms.untracked.int32(numEventsToRun)
)

process.MessageLogger.destinations = ['cout', 'cerr']
process.MessageLogger.cerr.FwkReport.reportEvery = 1
process.options   = cms.untracked.PSet( wantSummary = cms.untracked.bool(False) )

#process.options = cms.untracked.PSet( SkipEvent = cms.untracked.vstring('ProductNotFound')
#)
process.source = cms.Source("PoolSource", fileNames = cms.untracked.vstring(
    '/store/user/lnujj/PATtuples/sboutle/SingleMu/SQWaT_PAT_52X_2012A-PromptReco-v1_v1/6fdb4470d5dd47a73d67ddfb6436b825/pat_52x_test_1_1_vnQ.root'
) )




##-------- Electron events of interest --------
process.HLTMu =cms.EDFilter("HLTHighLevel",
     TriggerResultsTag = cms.InputTag("TriggerResults","","HLT"),
     HLTPaths = cms.vstring('HLT_IsoMu24_*','HLT_IsoMu30_*'),
     eventSetupPathsKey = cms.string(''),
     andOr = cms.bool(True), #----- True = OR, False = AND between the HLTPaths
     throw = cms.bool(False) # throw exception on unknown path names
)

process.primaryVertex.src = cms.InputTag("goodOfflinePrimaryVertices");
process.primaryVertex.cut = cms.string(" ");


##-------- Save V+jets trees --------
process.VplusJets = cms.EDAnalyzer("VplusJetsAnalysis", 
    jetType = cms.string("PF"),
  #  srcPFCor = cms.InputTag("selectedPatJetsPFlow"),
    srcPFCor = cms.InputTag("ak5PFJetsLooseId"),
    srcPhoton = cms.InputTag("photons"),
    IsoValPhoton = cms.VInputTag(cms.InputTag('phoPFIso:chIsoForGsfEle'),
                                 cms.InputTag('phoPFIso:phIsoForGsfEle'),
                                 cms.InputTag('phoPFIso:nhIsoForGsfEle'),
                                                           ),
    srcPFCorVBFTag = cms.InputTag("ak5PFJetsLooseIdVBFTag"), 
    srcVectorBoson = cms.InputTag("bestWmunu"),
    VBosonType     = cms.string('W'),
    LeptonType     = cms.string('muon'),                          
    HistOutFile = cms.string( OutputFileName ),
    TreeName    = cms.string('WJet'),
    srcPrimaryVertex = cms.InputTag("goodOfflinePrimaryVertices"),                               
    runningOverMC = cms.bool(isMC),			
    runningOverAOD = cms.bool(False),			
    srcMet = cms.InputTag("patMETsPFlow"),
    srcGen  = cms.InputTag("ak5GenJets"),
    srcMuons  = cms.InputTag("selectedPatMuonsPFlow"),
    srcBeamSpot  = cms.InputTag("offlineBeamSpot"),
    srcCaloMet  = cms.InputTag("patMETs"),
    srcgenMet  = cms.InputTag("genMetTrue"),
    srcGenParticles  = cms.InputTag("genParticles"),
    srcTcMet    = cms.InputTag("patMETsAK5TC"),
    srcJetsforRho = cms.string("kt6PFJetsPFlow"),                               
    srcJetsforRho_lepIso = cms.string("kt6PFJetsForIsolation"),       
    srcJetsforRhoCHS = cms.string("kt6PFJetsChsPFlow"),
    srcJetsforRho_lepIsoCHS = cms.string("kt6PFJetsChsForIsolationPFlow"),
    srcFlavorByValue = cms.InputTag("ak5tagJet"),
    bTagger=cms.string("simpleSecondaryVertexHighEffBJetTags"),
)



process.myseq = cms.Sequence(
    process.TrackVtxPath *
    process.HLTMu *
    process.WPath *
    process.GenJetPath *
##    process.btagging * 
    process.TagJetPath *
    process.PFJetPath 
    )

if isMC:
    process.myseq.remove ( process.noscraping)
    process.myseq.remove ( process.HBHENoiseFilter)
    process.myseq.remove ( process.HLTMu)
else:
    process.myseq.remove ( process.noscraping)
    process.myseq.remove ( process.HBHENoiseFilter)
    process.myseq.remove ( process.GenJetPath)
    process.myseq.remove ( process.TagJetPath)


##---- if do not want to require >= 2 jets then disable that filter ---
##process.myseq.remove ( process.RequireTwoJets)  

#process.outpath.remove(process.out)
process.p = cms.Path( process.myseq  * process.VplusJets)






