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

/* $Id:  */

/* $Log:
   29.05.2001 Yuri Kharlov:
              Everywhere reading the treese TTree->GetEvent(i)
              is replaced by reading the branches TBranch->GetEntry(0)
*/

//_________________________________________________________________________
//  A singleton. This class should be used in the analysis stage to get 
//  reconstructed objects: Digits, RecPoints, TrackSegments and RecParticles,
//  instead of directly reading them from galice.root file. This container 
//  ensures, that one reads Digits, made of these particular digits, RecPoints, 
//  made of these particular RecPoints, TrackSegments and RecParticles. 
//  This becomes non trivial if there are several identical branches, produced with
//  different set of parameters. 
//
//  An example of how to use (see also class AliPHOSAnalyser):
//  for(Int_t irecp = 0; irecp < gime->NRecParticles() ; irecp++)
//     AliPHOSRecParticle * part = gime->RecParticle(1) ;
//     ................
//  please->GetEvent(event) ;    // reads new event from galice.root
//                  
//*-- Author: Yves Schutz (SUBATECH) & Dmitri Peressounko (RRC KI & SUBATECH)
//*--         Completely redesigned by Dmitri Peressounko March 2001  
//
//*-- YS June 2001 : renamed the original AliPHOSIndexToObject and make
//*--         systematic usage of TFolders without changing the interface        
//////////////////////////////////////////////////////////////////////////////


// --- ROOT system ---

#include "TFile.h"
#include "TTree.h"
#include "TROOT.h"
#include "TObjString.h"
#include "TFolder.h"
#include "TParticle.h"

// --- Standard library ---
#include <iostream.h>

// --- AliRoot header files ---

#include "AliRun.h"
#include "AliConfig.h"
#include "AliPHOSLoader.h"
#include "AliPHOS.h"
#include "AliPHOSDigitizer.h"
#include "AliPHOSSDigitizer.h"
#include "AliPHOSClusterizer.h"
#include "AliPHOSClusterizerv1.h"
#include "AliPHOSTrackSegmentMaker.h"
#include "AliPHOSTrackSegmentMakerv1.h"
#include "AliPHOSTrackSegment.h"
#include "AliPHOSPID.h" 
#include "AliPHOSPIDv1.h" 
#include "AliPHOSGeometry.h"

ClassImp(AliPHOSLoader)
  


const TString AliPHOSLoader::fgkHitsName("HITS");//Name for TClonesArray with hits from one event
const TString AliPHOSLoader::fgkSDigitsName("SDIGITS");//Name for TClonesArray 
const TString AliPHOSLoader::fgkDigitsName("DIGITS");//Name for TClonesArray 
const TString AliPHOSLoader::fgkEmcRecPointsName("EMCRECPOINTS");//Name for TClonesArray 
const TString AliPHOSLoader::fgkCpvRecPointsName("CPVRECPOINTS");//Name for TClonesArray 
const TString AliPHOSLoader::fgkTracksName("TRACKS");//Name for TClonesArray 
const TString AliPHOSLoader::fgkReconstrParticles("RECPARTICLES");//Name for TClonesArray

const TString AliPHOSLoader::fgkEmcRecPointsBranchName("PHOSEmcRP");//Name for TClonesArray 
const TString AliPHOSLoader::fgkCpvRecPointsBranchName("PHOSCpvRP");//Name for TClonesArray 

//____________________________________________________________________________ 
AliPHOSLoader::AliPHOSLoader()
 {
  fDebug = 0;
 }
//____________________________________________________________________________ 
AliPHOSLoader::AliPHOSLoader(const Char_t *detname,const Char_t *eventfoldername):
      AliLoader(detname,eventfoldername)
{
  fDebug=0;
}
//____________________________________________________________________________ 
AliPHOSLoader::~AliPHOSLoader(){

}

void AliPHOSLoader::CleanFolders()
 {
   AliLoader::CleanFolders();
   
   //the our stuff
   
 }

//____________________________________________________________________________ 
//____________________________________________________________________________ 
const AliPHOS * AliPHOSLoader::PHOS() 
{
  // returns the PHOS object 
  AliPHOS * phos = dynamic_cast<AliPHOS*>(GetModulesFolder()->FindObject(fDetectorName));
  if ( phos == 0x0) 
    if (fDebug)
      cout << "WARNING: AliPHOSLoader::PHOS -> PHOS module not found in Folders" << endl ; 
  return phos ; 
}  

//____________________________________________________________________________ 
const AliPHOSGeometry * AliPHOSLoader::PHOSGeometry() 
{
  AliPHOSGeometry * rv = 0 ; 
  if (PHOS() )
    rv =  PHOS()->GetGeometry();
  return rv ; 
} 


//____________________________________________________________________________ 
Int_t AliPHOSLoader::PostHits(void)
{  
//------- Hits ----------------------
//Overload (extends) PostHits implemented in AliLoader
//

  Int_t res;
  
  //First call the AliLoader's method to send the TreeH to folder
  res = AliLoader::PostHits();
  if (res)
   {//oops, error
     Error("PostHits","AliLoader::PostHits returned error");
     return res;
   }

  //read the data from tree in folder and send it to folder

  res = ReadHits();

  return 0;
}


//____________________________________________________________________________ 
Int_t AliPHOSLoader::PostSDigits()
{  //---------- SDigits -------------------------
  Int_t res;
  //First call the AliLoader's method to send the TreeS to folder
  res = AliLoader::PostSDigits();
  if (res)
   {//oops, error
     Error("PostSDigits","AliLoader::PostSDigits returned error");
     return res;
   }
  return ReadSDigits();  
   
} 
//____________________________________________________________________________ 
Int_t AliPHOSLoader::PostDigits()
{ 

  Int_t res;
  //First call the AliLoader's method to send the TreeS to folder
  res = AliLoader::PostDigits();
  if (res)
   {//oops, error
     Error("PostDigits","AliLoader::PostDigits returned error");
     return res;
   }
  return ReadDigits();
}
//____________________________________________________________________________ 
Int_t AliPHOSLoader::PostRecPoints() 
{ // -------------- RecPoints -------------------------------------------
  

  Int_t res;
  //First call the AliLoader's method to send the TreeS to folder
  res = AliLoader::PostRecPoints();
  if (res)
   {//oops, error
     Error("PostRecPoints","AliLoader::PostRecPoints returned error");
     return res;
   }

  TFolder * phosFolder = GetDetectorDataFolder();
  if ( phosFolder  == 0x0 ) 
   {
     Error("PostDigits","Can not get detector data folder");
     return 1;
   }

  PostReconstructioner();

  return ReadRecPoints();
}
//____________________________________________________________________________ 
Int_t  AliPHOSLoader::PostTracks()
{
 Int_t res;
  //First call the AliLoader's method to send the TreeS to folder
 res = AliLoader::PostTracks();
 if (res)
  {//oops, error
    Error("PostRecPoints","AliLoader::PostRecPoints returned error");
    return res;
  }
 //PostTracker();
 //return ReadTracks();
 Error("PostTracks","Do not forget to finish implementation of this method");
 return 1000;
}
//____________________________________________________________________________ 
Int_t AliPHOSLoader::ReadHits()
{
// If there is no Clones Array in folder creates it and sends to folder
// then tries to read
// Reads the first entry of PHOS branch in hit tree TreeH()
// Reads data from TreeH and stores it in TClonesArray that sits in DetectorDataFolder
//
  TClonesArray* hits;
  TObject** hitref = HitsRef();
    
  if(hitref == 0x0)
   {
     hits = new TClonesArray("AliPHOSHit",1000);
     hits->SetName(fgkHitsName);
     GetDetectorDataFolder()->Add(hits);
     hitref = HitsRef();
   }
  else
   {
     hits = dynamic_cast<TClonesArray*>(*hitref);
   }

  TTree* treeh = TreeH();
  
  if(treeh == 0)
   {
    Error("ReadTreeH"," Cannot read TreeH from folder");
    return 1;
  }
  
  TBranch * hitsbranch = treeh->GetBranch(fDetectorName);
  if (hitsbranch == 0) 
   {
    Error("ReadTreeH"," Cannot find branch PHOS"); 
    return 1;
  }
  
  
  if (hitsbranch->GetEntries() > 1)
   {
    TClonesArray * tempo =  new TClonesArray("AliPHOSHit",1000);

    hitsbranch->SetAddress(&tempo);
    Int_t index = 0 ; 
    Int_t i = 0 ;
    for (i = 0 ; i < hitsbranch->GetEntries(); i++) 
     {
      hitsbranch->GetEntry(i) ;
      Int_t j = 0 ;
      for ( j = 0 ; j < tempo->GetEntries() ; j++) 
       {
         AliPHOSHit* hit = (AliPHOSHit*)tempo->At(j); 
         new((*hits)[index]) AliPHOSHit( *hit ) ;
         index++ ; 
       }
     }
    delete tempo;
   }
  else 
   {
    hitsbranch->SetAddress(hitref);
    hitsbranch->GetEntry(0) ;
   }

  return 0;
}
//____________________________________________________________________________ 
Int_t AliPHOSLoader::ReadSDigits()
{
// Read the summable digits tree TreeS():
// Check if TClones is in folder
// if not create and add to folder
// connect to tree if available
// Read the data

  TObject** sdref = SDigitsRef();
  TClonesArray* sdigits;
    
  if(sdref == 0x0)
   {
     sdigits = new TClonesArray("AliPHOSDigit",1);
     sdigits->SetName(fgkSDigitsName);
     GetDetectorDataFolder()->Add(sdigits);
     sdref = SDigitsRef();
   }
  else
   {
    sdigits = dynamic_cast<TClonesArray*>(*sdref);
   }
  TTree * treeS = TreeS();
  if(treeS==0)
   {
     //May happen if file is truncated or new in LoadSDigits
     Error("ReadSDigits","There is no SDigit Tree");
     return 0;
   }
  
  TBranch * branch = treeS->GetBranch(fDetectorName);
  if (branch == 0) 
   {//easy, maybe just a new tree
    Error("ReadSDigits"," Cannot find branch PHOS"); 
    return 0;
  }
    
  branch->SetAddress(SDigitsRef());
  branch->GetEntry(0);
  return 0;
}

//____________________________________________________________________________ 
Int_t AliPHOSLoader::ReadDigits()
{
// Read the summable digits tree TreeS():
// Check if TClones is in folder
// if not create and add to folder
// connect to tree if available
// Read the data
  
  TObject** dref = DigitsRef();
  TClonesArray* digits = dynamic_cast<TClonesArray*>(*dref);

  if(dref == 0x0)
   {//if there is not array in folder, create it and put it there
     digits = new TClonesArray("AliPHOSDigit",1000);
     digits->SetName(fgkDigitsName);
     GetDetectorDataFolder()->Add(digits);
     dref = DigitsRef();
   }

  TTree * treeD = TreeD();
  if(treeD==0)
   {
     //May happen if file is truncated or new in LoadSDigits
     Error("ReadDigits","There is no Digit Tree");
     return 0;
   }

  TBranch * branch = treeD->GetBranch(fDetectorName);
  if (branch == 0) 
   {//easy, maybe just a new tree
    Error("ReadDigits"," Cannot find branch ",fDetectorName.Data()); 
    return 0;
   }
  
  branch->SetAddress(dref);//connect branch to buffer sitting in folder
  branch->GetEntry(0);//get first event 

  return 0;  
}


//____________________________________________________________________________ 
void AliPHOSLoader::Track(Int_t itrack)
{
// Read the first entry of PHOS branch in hit tree gAlice->TreeH()
  if(TreeH()== 0)
   {
     if (LoadHits())
      {
        Error("Track","Can not load hits.");
        return;
      } 
   }
  
  TBranch * hitsbranch = dynamic_cast<TBranch*>(TreeH()->GetListOfBranches()->FindObject("PHOS")) ;
  if ( !hitsbranch ) {
    if (fDebug)
      cout << "WARNING:  AliPHOSLoader::ReadTreeH -> Cannot find branch PHOS" << endl ; 
    return ;
  }  
  if(!Hits())
    PostHits() ;

  hitsbranch->SetAddress(HitsRef());
  hitsbranch->GetEntry(itrack);


}
//____________________________________________________________________________ 
void AliPHOSLoader::ReadTreeQA()
{
  // Read the digit tree gAlice->TreeQA()
  // so far only PHOS knows about this Tree  

  if(PHOS()->TreeQA()== 0){
    cerr <<   "ERROR: AliPHOSLoader::ReadTreeQA: can not read TreeQA " << endl ;
    return ;
  }
  
  TBranch * qabranch = PHOS()->TreeQA()->GetBranch("PHOS");
  if (!qabranch) { 
    if (fDebug)
      cout << "WARNING: AliPHOSLoader::ReadTreeQA -> Cannot find QA Alarms for PHOS" << endl ;
    return ; 
  }   
  
//  if(!Alarms()) PostQA();

  qabranch->SetAddress(AlarmsRef()) ;

  qabranch->GetEntry(0) ;
  
}

//____________________________________________________________________________ 
Int_t AliPHOSLoader::ReadRecPoints()
{
//Creates and posts to folder an array container, 
//connects branch in tree (if exists), and reads data to arry

  TObject** cpvref = DigitsRef();
  
  if ( cpvref == 0x0 )   
   {
    TObjArray * cpv = new TObjArray(100) ;
    cpv->SetName(fgkCpvRecPointsName);
    GetDetectorDataFolder()->Add(cpv);
    cpvref = DigitsRef();
   }
  
  TObject** emcref = DigitsRef();
  if ( emcref == 0x0 ) 
   {
    TObjArray * emc = new TObjArray(100) ;
    emc->SetName(fgkEmcRecPointsName) ;
    GetDetectorDataFolder()->Add(emc);
    emcref = DigitsRef();
   }

  TTree * treeR = TreeR();
  if(treeR==0)
   {
     //May happen if file is truncated or new in LoadSDigits
     Error("ReadRecPoints","There is no Tree with Rec Points");
     return 0;
   }


  TBranch * emcbranch = treeR->GetBranch(fgkEmcRecPointsBranchName);
  emcbranch->SetAddress(emcref) ;
  emcbranch->GetEntry(0) ;
  
  TBranch * cpvbranch = treeR->GetBranch(fgkCpvRecPointsBranchName);
  cpvbranch->SetAddress(cpvref);
  cpvbranch->GetEntry(0) ;

  return 0;
}
//____________________________________________________________________________ 

Int_t AliPHOSLoader::ReadTracks()
{
//Creates and posts to folder an array container, 
//connects branch in tree (if exists), and reads data to arry

  TObject** trkref = TracksRef();
  
  if ( trkref == 0x0 )   
   {//Create and post array
    TClonesArray * ts = new TClonesArray("AliPHOSTrackSegment",100) ;
    ts->SetName(fgkTracksName);
    GetDetectorDataFolder()->Add(ts);
    trkref = TracksRef();
   }

  TTree * treeT = TreeT();
  if(treeT==0)
   {
     //May happen if file is truncated or new in LoadSDigits
     Error("ReadTracks","There is no Tree with Tracks");
     return 0;
   }
  
  TBranch * branch = treeT->GetBranch(fDetectorName);
  if (branch == 0) 
   {//easy, maybe just a new tree
    Error("ReadTracks"," Cannot find branch ",fDetectorName.Data()); 
    return 0;
  }

  branch->SetAddress(trkref);//connect branch to buffer sitting in folder
  branch->GetEntry(0);//get first event 
  
  return 0;
}

Int_t AliPHOSLoader::ReadRecParticles()
{
//Reads Reconstructed  Particles from file
//Creates and posts to folder an array container, 
//connects branch in tree (if exists), and reads data to arry
  
  Error("ReadRecParticles","Do not forgot to implement SKOWRON");
  return 1;
}
//____________________________________________________________________________ 

Int_t AliPHOSLoader::LoadRecParticles(Option_t* opt)
{
 return 0;
}
//____________________________________________________________________________ 
Int_t AliPHOSLoader::WriteRecParticles(Option_t* opt)
{
 return 0;
}
//____________________________________________________________________________ 
Int_t AliPHOSLoader::WritePID(Option_t* opt)
{
 return 0;
}
/********************************************************************************************************/
AliPHOSGeometry* AliPHOSLoader::GetPHOSGeometry()
{
//returns PHOS geometry from gAlice 
//static Method used by some classes where it is not convienient to pass eventfoldername
 if (gAlice == 0x0)
  return 0x0;
 AliPHOS* phos=dynamic_cast<AliPHOS*>(gAlice->GetDetector("PHOS"));
 if (phos == 0x0)
  return 0x0;
 return phos->GetGeometry();
}
/********************************************************************************************************/
AliPHOSLoader* AliPHOSLoader::GetPHOSLoader(const  char* eventfoldername)
{
  AliRunLoader* rn  = AliRunLoader::GetRunLoader(eventfoldername);
  if (rn == 0x0)
   {
     cerr<<"Error: <AliPHOSLoader::GetPHOSLoader>: "
         << "Can not find Run Loader in folder "<<eventfoldername<<endl;
     return 0x0;
   }
  return dynamic_cast<AliPHOSLoader*>(rn->GetLoader("PHOSLoader"));
}
/********************************************************************************************************/



