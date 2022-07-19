/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzCore/Math/Vector3.h>
#include <SceneAPI/SceneBuilder/SceneSystem.h>
#include <SceneAPI/SceneCore/Utilities/Reporting.h>
#include <SceneAPI/SDKWrapper/AssImpSceneWrapper.h>
#include <SceneAPI/SDKWrapper/AssImpTypeConverter.h>
#include <assimp/scene.h>


namespace AZ
{
    namespace SceneAPI
    {
        SceneSystem::SceneSystem() :
            m_unitSizeInMeters(1.0f),
            m_originalUnitSizeInMeters(1.0f),
            m_adjustTransform(nullptr),
            m_adjustTransformInverse(nullptr)
        {
        }

        void SceneSystem::Set(const SDKScene::SceneWrapperBase* scene)
        {
            // Get unit conversion factor to meter.
            if (!azrtti_istypeof<AssImpSDKWrapper::AssImpSceneWrapper>(scene))
            {
                return;
            }

            const AssImpSDKWrapper::AssImpSceneWrapper* assImpScene = azrtti_cast<const AssImpSDKWrapper::AssImpSceneWrapper*>(scene);

            // If either meta data piece is not available, the default of 1 will be used.
            assImpScene->GetAssImpScene()->mMetaData->Get("UnitScaleFactor", m_unitSizeInMeters);
            assImpScene->GetAssImpScene()->mMetaData->Get("OriginalUnitScaleFactor", m_originalUnitSizeInMeters);

            /* Conversion factor for converting from centimeters to meters */
            m_unitSizeInMeters = m_unitSizeInMeters * .01f;

            auto [upAxis, upAxisSign] = assImpScene->GetUpVectorAndSign();
            auto [frontAxis, frontAxisSign] = assImpScene->GetFrontVectorAndSign();

            // Get the up and forward direction vector of the loaded model.
            // Note that the *forward* direction (forward direction of the coordinate system) is the
            // reverse of fbx's *front* direction (or "viewer" direction), which points towards the
            // observer, i.e. towards the backwards direction of the coordinate system.
            AZ::Vector4 upVec(0.0f);
            AZ::Vector4 forwardVec(0.0f);
            upVec.SetElement(static_cast<int32_t>(upAxis), upAxisSign);
            forwardVec.SetElement(static_cast<int32_t>(frontAxis), -frontAxisSign);

            // Get a side vector by setting the up and front components to zero
            AZ::Vector4 sideVecNoSign(1.0f, 1.0f, 1.0f, 0.0f);
            sideVecNoSign.SetElement(static_cast<int32_t>(upAxis), 0.0f);
            sideVecNoSign.SetElement(static_cast<int32_t>(frontAxis), 0.0f);

            AZ::Vector4 wVec(0.0f, 0.0f, 0.0f, 1.0f);

            // Conversion to the native O3DE coordinate system.
            // Goal: +Z up, +Y forward (aka '-Y front'), keep the system right handed (i.e. det(adjustmatrix) == +1)
            // -> Shuffle the basis vectors in the order that we want them for our coordinate system
            AZ::Matrix4x4 adjustmatrix    = AZ::Matrix4x4::CreateFromRows( sideVecNoSign, forwardVec, upVec, wVec);
            AZ::Matrix4x4 adjustmatrixNeg = AZ::Matrix4x4::CreateFromRows(-sideVecNoSign, forwardVec, upVec, wVec);
            if (AssImpSDKWrapper::AssImpTypeConverter::ToTransform(adjustmatrix).GetDeterminant3x3() < 0)
            {
                adjustmatrix = adjustmatrixNeg; // the side vector needs a negative sign to preserve right-handedness
            }

            m_adjustTransform.reset(new DataTypes::MatrixType(AssImpSDKWrapper::AssImpTypeConverter::ToTransform(adjustmatrix)));
            m_adjustTransformInverse.reset(new DataTypes::MatrixType(m_adjustTransform->GetInverseFull()));
        }

        void SceneSystem::SwapVec3ForUpAxis(Vector3& swapVector) const
        {
            if (m_adjustTransform)
            {
                swapVector = *m_adjustTransform * swapVector;
            }
        }

        void SceneSystem::SwapTransformForUpAxis(DataTypes::MatrixType& inOutTransform) const
        {
            if (m_adjustTransform)
            {
                inOutTransform = (*m_adjustTransform * inOutTransform) * *m_adjustTransformInverse;
            }
        }

        void SceneSystem::ConvertUnit(Vector3& scaleVector) const
        {
            scaleVector *= m_unitSizeInMeters;
        }

        void SceneSystem::ConvertUnit(DataTypes::MatrixType& inOutTransform) const
        {
            Vector3 translation = inOutTransform.GetTranslation();
            translation *= m_unitSizeInMeters;
            inOutTransform.SetTranslation(translation);
        }

        void SceneSystem::ConvertBoneUnit(DataTypes::MatrixType& inOutTransform) const
        {


            // Need to scale translation explicitly as MultiplyByScale won't change the translation component
            // and we need to convert to meter unit
            Vector3 translation = inOutTransform.GetTranslation();
            translation *= m_unitSizeInMeters;
            inOutTransform.SetTranslation(translation);
        }
    }
}
