/**************************************************************************
 * Copyright(c) 1998-1999, ALICE Experiment at CERN, All rights reserved. *
 *                                                                        *
 * Author: The ALICE Off-line Project.                                    *
 * Contributors are mentioned in the code where appropriate.              *
 *                                                                        *
 * Permission to use, copy, modify and distribute this software and its   *
 * documentation strictly for non-commercial purposes is hereby granted   *
 * without fee, provided that the above copyright notice appears in all   *
 * copies and that both the copyright notice and this permission notice   *
 * appear in the supporting documentation. The authors make no claims     *
 * about the suitability of this software for any purpose. It is          *
 * provided "as is" without express or implied warranty.                  *
 **************************************************************************/

/* $Id$ */
//---------------------------------------------------------------------------------
//                          Class AliMCEventHandler
// This class gives access to MC truth during the analysis.
// Monte Carlo truth is containe in the kinematics tree (produced particles) and 
// the tree of reference hits.
//      
// Origin: Andreas Morsch, CERN, andreas.morsch@cern.ch 
//---------------------------------------------------------------------------------



#include "AliMCEventHandler.h"
#include "AliTrackReference.h"
#include "AliHeader.h"
#include "AliStack.h"
#include "AliLog.h"

#include <TTree.h>
#include <TFile.h>
#include <TParticle.h>
#include <TClonesArray.h>
#include <TDirectoryFile.h>
#include <TArrow.h>
#include <TMarker.h>
#include <TH2F.h>


ClassImp(AliMCEventHandler)

AliMCEventHandler::AliMCEventHandler() :
    AliVEventHandler(),
    fFileE(0),
    fFileK(0),
    fFileTR(0),
    fTmpFileTR(0),
    fTreeE(0),
    fTreeK(0),
    fTreeTR(0),
    fTmpTreeTR(0),
    fStack(0),
    fHeader(0),
    fTrackReferences(0),
    fNEvent(-1),
    fEvent(-1),
    fNprimaries(-1),
    fNparticles(-1),
    fPathName("./"),
    fExtension(""),
    fFileNumber(0),
    fEventsPerFile(0)
{
    // Default constructor
}

AliMCEventHandler::AliMCEventHandler(const char* name, const char* title) :
    AliVEventHandler(name, title),
    fFileE(0),
    fFileK(0),
    fFileTR(0),
    fTmpFileTR(0),
    fTreeE(0),
    fTreeK(0),
    fTreeTR(0),
    fTmpTreeTR(0),
    fStack(0),
    fHeader(new AliHeader()),
    fTrackReferences(new TClonesArray("AliTrackReference", 200)),
    fNEvent(-1),
    fEvent(-1),
    fNprimaries(-1),
    fNparticles(-1),
    fPathName("./"),
    fExtension(""),
    fFileNumber(0),
    fEventsPerFile(0)
{
    // Constructor
}
AliMCEventHandler::~AliMCEventHandler()
{ 
    // Destructor
    delete fFileE;
    delete fFileK;
    delete fFileTR;
}

Bool_t AliMCEventHandler::InitIO(Option_t* /*opt*/) 
{ 
    // Initialize input
    //
    fFileE = new TFile(Form("%sgalice.root", fPathName));
    if (!fFileE) AliFatal(Form("AliMCEventHandler:galice.root not found in directory %s ! \n", fPathName));

    fFileE->GetObject("TE", fTreeE);
    fTreeE->SetBranchAddress("Header", &fHeader);
    fNEvent = fTreeE->GetEntries();
    //
    // Tree K
    fFileK = new TFile(Form("%sKinematics%s.root", fPathName, fExtension));
    if (!fFileK) AliFatal(Form("AliMCEventHandler:Kinematics.root not found in directory %s ! \n", fPathName));
    fEventsPerFile = fFileK->GetNkeys() - fFileK->GetNProcessIDs();
    //
    // Tree TR
    fFileTR = new TFile(Form("%sTrackRefs%s.root", fPathName, fExtension));
    if (!fFileTR) AliWarning(Form("AliMCEventHandler:TrackRefs.root not found in directory %s ! \n", fPathName));
    //
    // Reset the event number
    fEvent      = -1;
    fFileNumber =  0;
    
    AliInfo(Form("AliMCEventHandler:Number of events in this directory %5d \n", fNEvent));
    return kTRUE;
    
}

Bool_t AliMCEventHandler::GetEvent(Int_t iev)
{
    // Load the event number iev
    //
    // Calculate the file number
    Int_t inew  = iev/fEventsPerFile;
    if (inew != fFileNumber) {
	fFileNumber = inew;
	if (!OpenFile(fFileNumber)){
	    return kFALSE;
	}
    }
    // Folder name
    char folder[20];
    sprintf(folder, "Event%d", iev);

    // TreeE
    fTreeE->GetEntry(iev);
    fStack = fHeader->Stack();
    // Tree K
    TDirectoryFile* dirK  = 0;
    fFileK->GetObject(folder, dirK);
    if (!dirK) {
	AliWarning(Form("AliMCEventHandler: Event #%5d not found\n", iev));
	return kFALSE;
    }
    dirK->GetObject("TreeK", fTreeK);
    fStack->ConnectTree(fTreeK);
    fStack->GetEvent();
    //Tree TR 
    if (fTreeTR) {
	TDirectoryFile* dirTR = 0;
	fFileTR->GetObject(folder, dirTR);
	dirTR->GetObject("TreeTR", fTreeTR);
	if (fTreeTR->GetBranch("AliRun")) {
	    // This is an old format with one branch per detector not in synch with TreeK
	    ReorderAndExpandTreeTR();
	} else {
	    // New format 
	    fTreeTR->SetBranchAddress("TrackReferences", &fTrackReferences);
	}
    }
    
    //
    fNparticles = fStack->GetNtrack();
    fNprimaries = fStack->GetNprimary();
    AliInfo(Form("AliMCEventHandler: Event#: %5d Number of particles %5d \n", fEvent, fNparticles));
    
    return kTRUE;
}

Bool_t AliMCEventHandler::OpenFile(Int_t i)
{
    // Open file i
    Bool_t ok = kTRUE;
    if (i > 0) {
	fExtension = Form("%d", i);
    } else {
	fExtension = "";
    }
    
    
    delete fFileK;
    fFileK = new TFile(Form("%sKinematics%s.root", fPathName, fExtension));
    if (!fFileK) {
	AliFatal(Form("AliMCEventHandler:Kinematics%s.root not found in directory %s ! \n", fExtension, fPathName));
	ok = kFALSE;
    }
    
    delete fFileTR;
    fFileTR = new TFile(Form("%sTrackRefs%s.root", fPathName, fExtension));
    if (!fFileTR) {
	AliWarning(Form("AliMCEventHandler:TrackRefs%s.root not found in directory %s ! \n", fExtension, fPathName));
	ok = kFALSE;
    }
    
    return ok;
}

Bool_t AliMCEventHandler::BeginEvent()
{ 
    // Read the next event
    fEvent++;
    if (fEvent >= fNEvent) {
	AliWarning(Form("AliMCEventHandler: Event number out of range %5d\n", fEvent));
	return kFALSE;
    }
    return GetEvent(fEvent);
}

Int_t AliMCEventHandler::GetParticleAndTR(Int_t i, TParticle*& particle, TClonesArray*& trefs)
{
    // Retrieve entry i
    if (i > -1 && i < fNparticles) {
	AliWarning(Form("AliMCEventHandler::GetEntry: Index out of range"));
	particle = 0;
	trefs    = 0;
	return (-1);
    }
    particle = fStack->Particle(i);
    if (fFileTR) {
	fTreeTR->GetEntry(fStack->TreeKEntry(i));
	trefs    = fTrackReferences;
	return trefs->GetEntries();
    } else {
	trefs = 0;
	return -1;
    }
}

void AliMCEventHandler::DrawCheck(Int_t i, Bool_t search)
{
    // Retrieve entry i and draw momentum vector and hits
    if (!fFileTR) {
	AliWarning("No Track Reference information available");
	return;
    } 

    if (i > -1 && i < fNparticles) {
	fTreeTR->GetEntry(fStack->TreeKEntry(i));
    } else {
	AliWarning("AliMCEventHandler::GetEntry: Index out of range");
    }
    
    Int_t nh = fTrackReferences->GetEntries();
    
    
    if (search) {
	while(nh == 0 && i < fNparticles - 1) {
	    i++;
	    fTreeTR->GetEntry(fStack->TreeKEntry(i));
	    nh =  fTrackReferences->GetEntries();
	}
	printf("Found Hits at %5d\n", i);
    }
    TParticle* particle = fStack->Particle(i);

    TH2F*    h = new TH2F("", "", 100, -500, 500, 100, -500, 500);
    Float_t x0 = particle->Vx();
    Float_t y0 = particle->Vy();

    Float_t x1 = particle->Vx() + particle->Px() * 50.;
    Float_t y1 = particle->Vy() + particle->Py() * 50.;
    
    TArrow*  a = new TArrow(x0, y0, x1, y1, 0.01);
    h->Draw();
    a->SetLineColor(2);
    
    a->Draw();
    
    for (Int_t ih = 0; ih < nh; ih++) {
	AliTrackReference* ref = (AliTrackReference*) fTrackReferences->At(ih);
	TMarker* m = new TMarker(ref->X(), ref->Y(), 20);
	m->Draw();
	m->SetMarkerSize(0.4);
	
    }
}

Bool_t AliMCEventHandler::Notify(const char *path)
{
    // Notify about directory change
    // The directory is taken from the 'path' argument
    // Reconnect trees

    printf("AliMCEventHandler::Notify() file: %s\n", path);
    fPathName = Form("%s",  path);
    ResetIO();
    InitIO("");
    return kTRUE;
}
    
void AliMCEventHandler::ResetIO()
{
    // Reset files
    if (fFileE)  delete fFileE;
    if (fFileK)  delete fFileK;
    if (fFileTR) delete fFileTR;
}

			    
Bool_t AliMCEventHandler::FinishEvent()
{
    // Dummy 
    return kTRUE;
}

Bool_t AliMCEventHandler::Terminate()
{ 
    // Dummy 
    return kTRUE;
}

Bool_t AliMCEventHandler::TerminateIO()
{ 
    // Dummy
    return kTRUE;
}
    
void AliMCEventHandler::ReorderAndExpandTreeTR()
{
//
//  Reorder and expand the track reference tree in order to match the kinematics tree.
//  Copy the information from different branches into one
//
//  TreeTR
    if (fTmpTreeTR) delete fTmpTreeTR;
    if (fTmpFileTR) {
	fTmpFileTR->Close();
	delete fTmpFileTR;
    }

    fTmpFileTR = new TFile("TrackRefsTmp.root", "recreate");
    fTmpTreeTR = new TTree("TreeTR", "Track References");
    if (!fTrackReferences)  fTrackReferences = new TClonesArray("AliTrackReference", 100);
    fTmpTreeTR->Branch("TrackReferences", "TClonesArray", &fTrackReferences, 4000);
//
    TClonesArray* trefs[7];
    for (Int_t i = 0; i < 7; i++) trefs[i] = 0;
    if (fTreeTR){
	// make branch for central track references
	if (fTreeTR->GetBranch("AliRun")) fTreeTR->SetBranchAddress("AliRun", &trefs[0]);
	if (fTreeTR->GetBranch("ITS"))    fTreeTR->SetBranchAddress("ITS",    &trefs[1]);
	if (fTreeTR->GetBranch("TPC"))    fTreeTR->SetBranchAddress("TPC",    &trefs[2]);
	if (fTreeTR->GetBranch("TRD"))    fTreeTR->SetBranchAddress("TRD",    &trefs[3]);
	if (fTreeTR->GetBranch("TOF"))    fTreeTR->SetBranchAddress("TOF",    &trefs[4]);
	if (fTreeTR->GetBranch("FRAME"))  fTreeTR->SetBranchAddress("FRAME",  &trefs[5]);
	if (fTreeTR->GetBranch("MUON"))   fTreeTR->SetBranchAddress("MUON",   &trefs[6]);
    }

    Int_t np = fStack->GetNprimary();
    Int_t nt = fTreeTR->GetEntries();
    //
    // Loop over tracks and find the secondaries with the help of the kine tree
    Int_t ifills = 0;
    Int_t it     = 0;
    Int_t itlast = 0;
    
    for (Int_t ip = np - 1; ip > -1; ip--) {
	TParticle *part = fStack->Particle(ip);
//	printf("Particle %5d %5d %5d %5d %5d %5d \n", 
//	       ip, part->GetPdgCode(), part->GetFirstMother(), part->GetFirstDaughter(), 
//	       part->GetLastDaughter(), part->TestBit(kTransportBit));

	// Determine range of secondaries produced by this primary during transport	
	Int_t dau1  = part->GetFirstDaughter();
	if (dau1 < np) continue;  // This particle has no secondaries produced during transport
	Int_t dau2  = -1;
	if (dau1 > -1) {
	    Int_t inext = ip - 1;
	    while (dau2 < 0) {
		if (inext >= 0) {
		    part = fStack->Particle(inext);
		    dau2 =  part->GetFirstDaughter();
		    if (dau2 == -1 || dau2 < np) {
			dau2 = -1;
		    } else {
			dau2--;
		    }
		} else {
		    dau2 = fStack->GetNtrack() - 1;
		}
		inext--;
	    } // find upper bound
	}  // dau2 < 0
	

//	printf("Check (1) %5d %5d %5d %5d %5d \n", ip, np, it, dau1, dau2);
//
// Loop over reference hits and find secondary label
// First the tricky part: find the entry in treeTR than contains the hits or
// make sure that no hits exist.
//
	Bool_t hasHits   = kFALSE;
	Bool_t isOutside = kFALSE;

	it = itlast;
	while (!hasHits && !isOutside && it < nt) {
	    fTreeTR->GetEntry(it++);
	    for (Int_t ib = 0; ib < 7; ib++) {
		if (!trefs[ib]) continue;
		Int_t nh = trefs[ib]->GetEntries();
		for (Int_t ih = 0; ih < nh; ih++) {
		    AliTrackReference* tr = (AliTrackReference*) trefs[ib]->At(ih);
		    Int_t label = tr->Label();
		    if (label >= dau1 && label <= dau2) {
			hasHits = kTRUE;
			itlast = it - 1;
			break;
		    }
		    if (label > dau2 || label < ip) {
			isOutside = kTRUE;
			itlast = it - 1;
			break;
		    }
		} // hits
		if (hasHits || isOutside) break;
	    } // branches
	} // entries

	if (!hasHits) {
	    // Write empty entries
	    for (Int_t id = dau1; (id <= dau2); id++) {
		fTmpTreeTR->Fill();
		ifills++;
	    } 
	} else {
	    // Collect all hits
	    fTreeTR->GetEntry(itlast);
	    for (Int_t id = dau1; (id <= dau2) && (dau1 > -1); id++) {
		for (Int_t ib = 0; ib < 7; ib++) {
		    if (!trefs[ib]) continue;
		    Int_t nh = trefs[ib]->GetEntries();
		    for (Int_t ih = 0; ih < nh; ih++) {
			AliTrackReference* tr = (AliTrackReference*) trefs[ib]->At(ih);
			Int_t label = tr->Label();
			// Skip primaries
			if (label == ip) continue;
			if (label > dau2 || label < dau1) 
			    printf("AliMCEventHandler::Track Reference Label out of range !: %5d %5d %5d %5d \n", 
				   itlast, label, dau1, dau2);
			if (label == id) {
			    // secondary found
			    tr->SetDetectorId(ib-1);
			    Int_t nref =  fTrackReferences->GetEntriesFast();
			    TClonesArray &lref = *fTrackReferences;
			    new(lref[nref]) AliTrackReference(*tr);
			}
		    } // hits
		} // branches
		fTmpTreeTR->Fill();
		fTrackReferences->Clear();
		ifills++;
	    } // daughters
	} // has hits
    } // tracks
    //
    // Now loop again and write the primaries
    //
    it = nt - 1;
    for (Int_t ip = 0; ip < np; ip++) {
	Int_t labmax = -1;
	while (labmax < ip && it > -1) {
	    fTreeTR->GetEntry(it--);
	    for (Int_t ib = 0; ib < 7; ib++) {
		if (!trefs[ib]) continue;
		Int_t nh = trefs[ib]->GetEntries();
		// 
		// Loop over reference hits and find primary labels
		for (Int_t ih = 0; ih < nh; ih++) {
		    AliTrackReference* tr = (AliTrackReference*)  trefs[ib]->At(ih);
		    Int_t label = tr->Label();
		    if (label < np && label > labmax) {
			labmax = label;
		    }
		    
		    if (label == ip) {
			tr->SetDetectorId(ib-1);
			Int_t nref = fTrackReferences->GetEntriesFast();
			TClonesArray &lref = *fTrackReferences;
			new(lref[nref]) AliTrackReference(*tr);
		    }
		} // hits
	    } // branches
	} // entries
	it++;
	fTmpTreeTR->Fill();
	fTrackReferences->Clear();
	ifills++;
    } // tracks
    // Check
    if (ifills != fStack->GetNtrack()) 
	printf("AliMCEventHandler:Number of entries in TreeTR (%5d) unequal to TreeK (%5d) \n", 
	       ifills, fStack->GetNtrack());
    fTreeTR = fTmpTreeTR;
}
