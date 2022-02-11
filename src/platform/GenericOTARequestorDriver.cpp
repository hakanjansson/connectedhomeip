/*
 *
 *    Copyright (c) 2021 Project CHIP Authors
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#include "GenericOTARequestorDriver.h"

#include <platform/CHIPDeviceLayer.h>
#include <platform/OTAImageProcessor.h>
#include <platform/OTARequestorInterface.h>

namespace chip {
namespace DeviceLayer {
namespace {

using namespace app::Clusters::OtaSoftwareUpdateRequestor;

GenericOTARequestorDriver * ToDriver(void * context)
{
    return static_cast<GenericOTARequestorDriver *>(context);
}

constexpr System::Clock::Seconds32 kDefaultDelayedActionTime = System::Clock::Seconds32(120);

} // namespace

bool GenericOTARequestorDriver::CanConsent()
{
    return false;
}

uint16_t GenericOTARequestorDriver::GetMaxDownloadBlockSize()
{
    return 1024;
}

void GenericOTARequestorDriver::HandleError(UpdateFailureState state, CHIP_ERROR error)
{
   
}

void GenericOTARequestorDriver::UpdateAvailable(const UpdateDescription & update, System::Clock::Seconds32 delay)
{
    VerifyOrDie(mRequestor != nullptr);
    ScheduleDelayedAction(UpdateFailureState::kDownloading, delay,
                          [](System::Layer *, void * context) { ToDriver(context)->mRequestor->DownloadUpdate(); }, this);
}

void GenericOTARequestorDriver::UpdateNotFound(UpdateNotFoundReason reason, System::Clock::Seconds32 delay)
{
    VerifyOrDie(mRequestor != nullptr);

    if (delay < kDefaultDelayedActionTime)
    {
        delay = kDefaultDelayedActionTime;
    }

    ScheduleDelayedAction(UpdateFailureState::kQuerying, delay,
                          [](System::Layer *, void * context) { ToDriver(context)->mRequestor->TriggerImmediateQuery(); }, this);
}

void GenericOTARequestorDriver::UpdateDownloaded()
{
    VerifyOrDie(mRequestor != nullptr);
    mRequestor->ApplyUpdate();
}

void GenericOTARequestorDriver::UpdateConfirmed(System::Clock::Seconds32 delay)
{
    VerifyOrDie(mImageProcessor != nullptr);
    ScheduleDelayedAction(UpdateFailureState::kApplying, delay,
                          [](System::Layer *, void * context) { ToDriver(context)->mImageProcessor->Apply(); }, this);
}

void GenericOTARequestorDriver::UpdateSuspended(System::Clock::Seconds32 delay)
{
    VerifyOrDie(mRequestor != nullptr);

    if (delay < kDefaultDelayedActionTime)
    {
        delay = kDefaultDelayedActionTime;
    }

    ScheduleDelayedAction(UpdateFailureState::kAwaitingNextAction, delay,
                          [](System::Layer *, void * context) { ToDriver(context)->mRequestor->ApplyUpdate(); }, this);
}

void GenericOTARequestorDriver::UpdateDiscontinued()
{
    VerifyOrDie(mImageProcessor != nullptr);
    mImageProcessor->Abort();
}

void GenericOTARequestorDriver::UpdateCancelled()
{
    // Platform might choose to schedule a retry here
}

void GenericOTARequestorDriver::ScheduleDelayedAction(UpdateFailureState state, System::Clock::Seconds32 delay,
                                                      System::TimerCompleteCallback action, void * aAppState)
{
    CHIP_ERROR error = SystemLayer().StartTimer(std::chrono::duration_cast<System::Clock::Timeout>(delay), action, aAppState);

    if (error != CHIP_NO_ERROR)
    {
        HandleError(state, error);
    }
}

void GenericOTARequestorDriver::CancelDelayedAction(System::TimerCompleteCallback action, void * aAppState)
{
    SystemLayer().CancelTimer(action, aAppState);
}


OTARequestorAction GenericOTARequestorDriver::GetRequestorAction(OTARequestorIncomingEvent input)
{

    OTAUpdateStateEnum state  = mRequestor->GetCurrentUpdateState();
    OTARequestorAction action = OTARequestorAction::DoNotProceed;

    switch(input)
        {
            case OTARequestorIncomingEvent::AnnouncedOTAProviderReceived:
                {
                    if(state != OTAUpdateStateEnum::kIdle) {
                        action = OTARequestorAction::DoNotProceed;
                    } else {
                        action = OTARequestorAction::Proceed;
                    }

                    break;
                }
            case OTARequestorIncomingEvent::TriggerImmediateQueryInvoked:
                {
                    if(state != OTAUpdateStateEnum::kIdle) {
                        action = OTARequestorAction::DoNotProceed;
                    } else {
                        action = OTARequestorAction::Proceed;
                    }
                    break;
                }
            case OTARequestorIncomingEvent::DefaultProvidersAttrSet:
                {
                    if(state != OTAUpdateStateEnum::kIdle) {
                        action = OTARequestorAction::CancelCurrentUpdateAndProceed;
                    } else {
                        action = OTARequestorAction::Proceed;
                    }
                    break;
                }
            case OTARequestorIncomingEvent::DefaultProvidersTimerExpiry:
                {
                    action = OTARequestorAction:: Proceed;
                    break;
                }
            default:
                {
                    action = OTARequestorAction::Proceed;
                }
        }

    return action;
}



} // namespace DeviceLayer
} // namespace chip
