/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PannerNode_h_
#define PannerNode_h_

#include "AudioNode.h"
#include "AudioParam.h"
#include "mozilla/dom/PannerNodeBinding.h"
#include "ThreeDPoint.h"
#include "mozilla/WeakPtr.h"
#include <limits>
#include <set>

namespace mozilla {
namespace dom {

class AudioContext;
class AudioBufferSourceNode;
struct PannerOptions;

class PannerNode final : public AudioNode,
                         public SupportsWeakPtr<PannerNode>
{
public:
  static already_AddRefed<PannerNode>
  Create(AudioContext& aAudioContext, const PannerOptions& aOptions,
         ErrorResult& aRv);

  MOZ_DECLARE_WEAKREFERENCE_TYPENAME(PannerNode)

  static already_AddRefed<PannerNode>
  Constructor(const GlobalObject& aGlobal, AudioContext& aAudioContext,
              const PannerOptions& aOptions, ErrorResult& aRv)
  {
    return Create(aAudioContext, aOptions, aRv);
  }

  JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  void SetChannelCount(uint32_t aChannelCount, ErrorResult& aRv) override
  {
    if (aChannelCount > 2) {
      aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
      return;
    }
    AudioNode::SetChannelCount(aChannelCount, aRv);
  }
  void SetChannelCountModeValue(ChannelCountMode aMode, ErrorResult& aRv) override
  {
    if (aMode == ChannelCountMode::Max) {
      aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
      return;
    }
    AudioNode::SetChannelCountModeValue(aMode, aRv);
  }

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(PannerNode, AudioNode)

  PanningModelType PanningModel() const
  {
    return mPanningModel;
  }
  void SetPanningModel(PanningModelType aPanningModel);

  DistanceModelType DistanceModel() const
  {
    return mDistanceModel;
  }
  void SetDistanceModel(DistanceModelType aDistanceModel)
  {
    mDistanceModel = aDistanceModel;
    SendInt32ParameterToStream(DISTANCE_MODEL, int32_t(mDistanceModel));
  }

  void SetPosition(double aX, double aY, double aZ)
  {
    if (fabs(aX) > std::numeric_limits<float>::max() ||
        fabs(aY) > std::numeric_limits<float>::max() ||
        fabs(aZ) > std::numeric_limits<float>::max()) {
      return;
    }
    mPositionX->SetValue(aX);
    mPositionY->SetValue(aY);
    mPositionZ->SetValue(aZ);
    SendThreeDPointParameterToStream(POSITION, ConvertAudioParamTo3DP(mPositionX, mPositionY, mPositionZ));
  }

  void SetOrientation(double aX, double aY, double aZ)
  {
    if (fabs(aX) > std::numeric_limits<float>::max() ||
        fabs(aY) > std::numeric_limits<float>::max() ||
        fabs(aZ) > std::numeric_limits<float>::max()) {
      return;
    }
    mOrientationX->SetValue(aX);
    mOrientationY->SetValue(aY);
    mOrientationZ->SetValue(aZ);
    SendThreeDPointParameterToStream(ORIENTATION, ConvertAudioParamTo3DP(mOrientationX, mOrientationY, mOrientationZ));
  }

  double RefDistance() const
  {
    return mRefDistance;
  }
  void SetRefDistance(double aRefDistance, ErrorResult& aRv)
  {
    if (WebAudioUtils::FuzzyEqual(mRefDistance, aRefDistance)) {
      return;
    }

    if (aRefDistance < 0) {
       aRv.template ThrowRangeError<
        MSG_INVALID_PANNERNODE_REFDISTANCE_ERROR>();
      return;
    }

    mRefDistance = aRefDistance;
    SendDoubleParameterToStream(REF_DISTANCE, mRefDistance);
  }

  double MaxDistance() const
  {
    return mMaxDistance;
  }
  void SetMaxDistance(double aMaxDistance, ErrorResult& aRv)
  {
    if (WebAudioUtils::FuzzyEqual(mMaxDistance, aMaxDistance)) {
      return;
    }

    if (aMaxDistance <= 0) {
       aRv.template ThrowRangeError<
        MSG_INVALID_PANNERNODE_MAXDISTANCE_ERROR>();
      return;
    }

    mMaxDistance = aMaxDistance;
    SendDoubleParameterToStream(MAX_DISTANCE, mMaxDistance);
  }

  double RolloffFactor() const
  {
    return mRolloffFactor;
  }
  void SetRolloffFactor(double aRolloffFactor, ErrorResult& aRv)
  {
    if (WebAudioUtils::FuzzyEqual(mRolloffFactor, aRolloffFactor)) {
      return;
    }


    if (aRolloffFactor < 0) {
       aRv.template ThrowRangeError<
        MSG_INVALID_PANNERNODE_ROLLOFF_ERROR>();
    }

    mRolloffFactor = aRolloffFactor;
    SendDoubleParameterToStream(ROLLOFF_FACTOR, mRolloffFactor);
  }

  double ConeInnerAngle() const
  {
    return mConeInnerAngle;
  }
  void SetConeInnerAngle(double aConeInnerAngle)
  {
    if (WebAudioUtils::FuzzyEqual(mConeInnerAngle, aConeInnerAngle)) {
      return;
    }
    mConeInnerAngle = aConeInnerAngle;
    SendDoubleParameterToStream(CONE_INNER_ANGLE, mConeInnerAngle);
  }

  double ConeOuterAngle() const
  {
    return mConeOuterAngle;
  }
  void SetConeOuterAngle(double aConeOuterAngle)
  {
    if (WebAudioUtils::FuzzyEqual(mConeOuterAngle, aConeOuterAngle)) {
      return;
    }
    mConeOuterAngle = aConeOuterAngle;
    SendDoubleParameterToStream(CONE_OUTER_ANGLE, mConeOuterAngle);
  }

  double ConeOuterGain() const
  {
    return mConeOuterGain;
  }
  void SetConeOuterGain(double aConeOuterGain, ErrorResult& aRv)
  {
    if (WebAudioUtils::FuzzyEqual(mConeOuterGain, aConeOuterGain)) {
      return;
    }

    if (aConeOuterGain < 0 || aConeOuterGain > 1) {
      aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
      return;
    }

    mConeOuterGain = aConeOuterGain;
    SendDoubleParameterToStream(CONE_OUTER_GAIN, mConeOuterGain);
  }

  AudioParam* PositionX()
  {
    return mPositionX;
  }

  AudioParam* PositionY()
  {
    return mPositionY;
  }

  AudioParam* PositionZ()
  {
    return mPositionZ;
  }

  AudioParam* OrientationX()
  {
    return mOrientationX;
  }

  AudioParam* OrientationY()
  {
    return mOrientationY;
  }

  AudioParam* OrientationZ()
  {
    return mOrientationZ;
  }

  const char* NodeType() const override
  {
    return "PannerNode";
  }

  size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const override;
  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override;

private:
  explicit PannerNode(AudioContext* aContext);
  ~PannerNode() = default;

  friend class AudioListener;
  friend class PannerNodeEngine;
  enum EngineParameters {
    PANNING_MODEL,
    DISTANCE_MODEL,
    POSITION,
    POSITIONX,
    POSITIONY,
    POSITIONZ,
    ORIENTATION, // unit length or zero
    ORIENTATIONX,
    ORIENTATIONY,
    ORIENTATIONZ,
    REF_DISTANCE,
    MAX_DISTANCE,
    ROLLOFF_FACTOR,
    CONE_INNER_ANGLE,
    CONE_OUTER_ANGLE,
    CONE_OUTER_GAIN
  };

  ThreeDPoint ConvertAudioParamTo3DP(RefPtr <AudioParam> aX, RefPtr <AudioParam> aY, RefPtr <AudioParam> aZ)
  {
    return ThreeDPoint(aX->GetValue(), aY->GetValue(), aZ->GetValue());
  }

  PanningModelType mPanningModel;
  DistanceModelType mDistanceModel;
  RefPtr<AudioParam> mPositionX;
  RefPtr<AudioParam> mPositionY;
  RefPtr<AudioParam> mPositionZ;
  RefPtr<AudioParam> mOrientationX;
  RefPtr<AudioParam> mOrientationY;
  RefPtr<AudioParam> mOrientationZ;

  double mRefDistance;
  double mMaxDistance;
  double mRolloffFactor;
  double mConeInnerAngle;
  double mConeOuterAngle;
  double mConeOuterGain;
};

} // namespace dom
} // namespace mozilla

#endif
