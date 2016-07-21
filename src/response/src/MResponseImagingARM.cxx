/*
 * MResponseImagingARM.cxx
 *
 *
 * Copyright (C) by Andreas Zoglauer.
 * All rights reserved.
 *
 *
 * This code implementation is the intellectual property of
 * Andreas Zoglauer.
 *
 * By copying, distributing or modifying the Program (or any work
 * based on the Program) you indicate your acceptance of this statement,
 * and all its terms.
 *
 */


////////////////////////////////////////////////////////////////////////////////
//
// MResponseImagingARM
//
////////////////////////////////////////////////////////////////////////////////


// Include the header:
#include "MResponseImagingARM.h"

// Standard libs:
#include <vector>
using namespace std;

// ROOT libs:

// MEGAlib libs:
#include "MAssert.h"
#include "MStreams.h"
#include "MSettingsRevan.h"
#include "MSettingsMimrec.h"


////////////////////////////////////////////////////////////////////////////////


#ifdef ___CINT___
ClassImp(MResponseImagingARM)
#endif


////////////////////////////////////////////////////////////////////////////////


MResponseImagingARM::MResponseImagingARM()
{
  // Construct an instance of MResponseImagingARM
}


////////////////////////////////////////////////////////////////////////////////


MResponseImagingARM::~MResponseImagingARM()
{
  // Delete this instance of MResponseImagingARM
}


////////////////////////////////////////////////////////////////////////////////


bool MResponseImagingARM::OpenFiles()
{
  // Load the simulation file --- has to be called after the geometry is loaded

  m_ReReader = new MRawEventAnalyzer();
  m_ReReader->SetGeometry(m_ReGeometry);
  if (m_ReReader->SetInputModeFile(m_SimulationFileName) == false) return false;

  MSettingsRevan RevanCfg(false);
  RevanCfg.Read(m_RevanCfgFileName);
  m_ReReader->SetSettings(&RevanCfg);

  MSettingsMimrec MimrecCfg(false);
  MimrecCfg.Read(m_MimrecCfgFileName);
  m_MimrecEventSelector.SetSettings(&MimrecCfg);

  if (m_ReReader->PreAnalysis() == false) return false;

  m_SiReader = new MFileEventsSim(m_SiGeometry);
  if (m_SiReader->Open(m_SimulationFileName) == false) return false;

  return true;
}

////////////////////////////////////////////////////////////////////////////////


bool MResponseImagingARM::CreateResponse()
{
  // Create the multiple Compton response

  if ((m_SiGeometry = LoadGeometry(false, 0.0)) == 0) return false;
  if ((m_ReGeometry = LoadGeometry(true, 0.0)) == 0) return false;

  if (OpenFiles() == false) return false;

  cout<<"Generating imaging pdf"<<endl;

  vector<float> AxisPhiDiff;

  vector<float> Axis; // = CreateLogDist(0.1, 180, 20);
  for (float x = 0.25; x < 9.9; x += 0.25) Axis.push_back(x);
  for (float x = 10; x < 19.9; x += 0.5) Axis.push_back(x);
  for (float x = 20; x <= 179.9; x += 1) Axis.push_back(x);
  Axis.push_back(180.0);

  // Add Inverted:
  for (unsigned int b = Axis.size()-1; b < Axis.size(); --b) {
    AxisPhiDiff.push_back(-Axis[b]);
  }
  AxisPhiDiff.push_back(0);
  for (unsigned int b = 0; b < Axis.size(); ++b) {
    AxisPhiDiff.push_back(Axis[b]);
  }

  vector<float> Energy = CreateThresholdedLogDist(50, 10000, 50, 25);
  
  vector<float> Distance;
  Distance.push_back(0);
  Distance.push_back(0.19);
  Distance.push_back(0.39);
  Distance.push_back(0.69);
  Distance.push_back(0.99);
  Distance.push_back(1.49);
  Distance.push_back(1.99);
  Distance.push_back(2.99);
  Distance.push_back(4.99);
  Distance.push_back(9.99);
  Distance.push_back(19.99);
  Distance.push_back(99.99);
  
  MResponseMatrixO3 Arm("AngularResolution)", AxisPhiDiff, Energy, Distance);
  Arm.SetAxisNames("#phi_{meas} - #phi_{real} [deg]", "Measured energy [keV]", "Distance between first 2 interactions [cm]");
  
  MResponseMatrixO3 ArmPhotoPeak("AngularResolution (photo_peak)", AxisPhiDiff, Energy, Distance);
  ArmPhotoPeak.SetAxisNames("#phi_{meas} - #phi_{real} [deg]", "Measured energy [keV]", "Distance between first 2 interactions [cm]");


  double PhiDiff;
  MVector IdealOrigin;
  double IdealEnergy;

  MRawEventList* REList = 0;
  MPhysicalEvent* Event = 0;
  MComptonEvent* Compton = 0;
  
  int Counter = 0;
  while (InitializeNextMatchingEvent() == true) {
    REList = m_ReReader->GetRawEventList();

    if (REList->HasOptimumEvent() == true) {
      Event = REList->GetOptimumEvent()->GetPhysicalEvent();
      if (Event != 0) {
        if (m_MimrecEventSelector.IsQualifiedEvent(Event) == true) {
          if (Event->GetType() == MPhysicalEvent::c_Compton) {
            Compton = (MComptonEvent*) Event;

            if (Compton->IsKinematicsOK() == false) continue;
            
            // Now get the ideal origin:
            if (m_SiEvent->GetNIAs() > 0) {
              IdealOrigin = m_SiEvent->GetIAAt(0)->GetPosition();

              // Phi response:
              PhiDiff = Compton->GetARMGamma(IdealOrigin)*c_Deg;
              
              //
              Arm.Add(PhiDiff, Compton->Ei(), Compton->LeverArm());
              
              IdealEnergy = m_SiEvent->GetIAAt(0)->GetSecondaryEnergy();
              
              if (IdealEnergy >= REList->GetOptimumEvent()->GetEnergy() - 3*REList->GetOptimumEvent()->GetEnergyResolution() &&
                  IdealEnergy <= REList->GetOptimumEvent()->GetEnergy() + 3*REList->GetOptimumEvent()->GetEnergyResolution()) {
                ArmPhotoPeak.Add(PhiDiff, Compton->Ei(), Compton->LeverArm());
              }
            }
          }
        }
      }    
    }
    if (++Counter % m_SaveAfter == 0) {
      Arm.Write(m_ResponseName + ".arm.allenergies" + m_Suffix, true);
      ArmPhotoPeak.Write(m_ResponseName + ".arm.photopeak" + m_Suffix, true);
    }
  }  

  Arm.Write(m_ResponseName + ".arm.allenergies" + m_Suffix, true);
  ArmPhotoPeak.Write(m_ResponseName + ".arm.photopeak" + m_Suffix, true);

  return true;
}


// MResponseImagingARM.cxx: the end...
////////////////////////////////////////////////////////////////////////////////
