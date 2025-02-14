/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AssetBuilderSDK/AssetBuilderSDK.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <Editor/Include/ScriptCanvas/Components/EditorGraph.h>
#include <Editor/ReflectComponent.h>
#include <Editor/View/Dialogs/SettingsDialog.h>
#include <Editor/View/Widgets/LoggingPanel/LiveWindowSession/LiveLoggingWindowSession.h>
#include <Editor/View/Widgets/NodePalette/CreateNodeMimeEvent.h>
#include <Editor/View/Widgets/NodePalette/EBusNodePaletteTreeItemTypes.h>
#include <Editor/View/Widgets/NodePalette/FunctionNodePaletteTreeItemTypes.h>
#include <Editor/View/Widgets/NodePalette/GeneralNodePaletteTreeItemTypes.h>
#include <Editor/View/Widgets/NodePalette/ScriptEventsNodePaletteTreeItemTypes.h>
#include <Editor/View/Widgets/NodePalette/SpecializedNodePaletteTreeItemTypes.h>
#include <Editor/View/Widgets/NodePalette/VariableNodePaletteTreeItemTypes.h>
#include <ScriptCanvas/Bus/UndoBus.h>
#include <ScriptCanvas/Components/EditorDeprecationData.h>
#include <ScriptCanvas/Components/EditorGraphVariableManagerComponent.h>
#include <ScriptCanvas/Variable/GraphVariableManagerComponent.h>

namespace CoreCpp
{
    static bool ScriptCanvasDataVersionConverter(AZ::SerializeContext& context, AZ::SerializeContext::DataElementNode& rootDataElementNode)
    {
        if (rootDataElementNode.GetVersion() == 0)
        {
            int scriptCanvasEntityIndex = rootDataElementNode.FindElement(AZ_CRC("m_scriptCanvas", 0xfcd20d85));
            if (scriptCanvasEntityIndex == -1)
            {
                AZ_Error("Script Canvas", false, "Version Converter failed, The Script Canvas Entity is missing");
                return false;
            }

            auto scComponentElements = AZ::Utils::FindDescendantElements(context, rootDataElementNode, AZStd::vector<AZ::Crc32>{AZ_CRC("m_scriptCanvas", 0xfcd20d85),
                AZ_CRC("element", 0x41405e39), AZ_CRC("Components", 0xee48f5fd)});
            if (!scComponentElements.empty())
            {
                scComponentElements.front()->AddElementWithData(context, "element", ScriptCanvasEditor::EditorGraphVariableManagerComponent());
            }
        }

        if (rootDataElementNode.GetVersion() < 4)
        {
            auto scEntityElements = AZ::Utils::FindDescendantElements(context, rootDataElementNode,
                AZStd::vector<AZ::Crc32>{AZ_CRC("m_scriptCanvas", 0xfcd20d85), AZ_CRC("element", 0x41405e39)});
            if (scEntityElements.empty())
            {
                AZ_Error("Script Canvas", false, "Version Converter failed, The Script Canvas Entity is missing");
                return false;
            }
            auto& scEntityDataElement = *scEntityElements.front();

            AZ::Entity scEntity;
            if (!scEntityDataElement.GetData(scEntity))
            {
                AZ_Error("Script Canvas", false, "Unable to retrieve entity data from the Data Element");
                return false;
            }

            auto graph = AZ::EntityUtils::FindFirstDerivedComponent<ScriptCanvas::Graph>(&scEntity);
            if (!graph)
            {
                AZ_Error("Script Canvas", false, "Script Canvas graph component could not be found on Script Canvas Entity for ScriptCanvasData version %u", rootDataElementNode.GetVersion());
                return false;
            }
            auto variableManager = AZ::EntityUtils::FindFirstDerivedComponent<ScriptCanvas::GraphVariableManagerComponent>(&scEntity);
            if (!variableManager)
            {
                AZ_Error("Script Canvas", false, "Script Canvas variable manager component could not be found on Script Canvas Entity for ScriptCanvasData version %u", rootDataElementNode.GetVersion());
                return false;
            }

            variableManager->ConfigureScriptCanvasId(graph->GetScriptCanvasId());
            if (!scEntityDataElement.SetData(context, scEntity))
            {
                AZ_Error("Script Canvas", false, "Failed to set converted Script Canvas Entity back on data element node when transitioning from version %u to version 4", rootDataElementNode.GetVersion());
                return false;
            }
        }

        return true;
    }
}

namespace ScriptCanvas
{
    void ScriptCanvasData::Reflect(AZ::ReflectContext* reflectContext)
    {
        if (auto serializeContext = azrtti_cast<AZ::SerializeContext*>(reflectContext))
        {
            serializeContext->Class<ScriptCanvasData>()
                ->Version(4, &CoreCpp::ScriptCanvasDataVersionConverter)
                ->Field("m_scriptCanvas", &ScriptCanvasData::m_scriptCanvasEntity)
                ;
        }
    }
}

namespace ScriptCanvasEditor
{
    void ReflectComponent::Reflect(AZ::ReflectContext* context)
    {
        SourceHandle::Reflect(context);
        ScriptCanvas::ScriptCanvasData::Reflect(context);
        Deprecated::ScriptCanvasAssetHolder::Reflect(context);
        EditorSettings::EditorWorkspace::Reflect(context);        
        EditorSettings::ScriptCanvasEditorSettings::Reflect(context);        
        LiveLoggingUserSettings::Reflect(context);
        UndoData::Reflect(context);

        // Base Mime Event
        CreateNodeMimeEvent::Reflect(context);
        SpecializedCreateNodeMimeEvent::Reflect(context);
        MultiCreateNodeMimeEvent::Reflect(context);

        // Specific Mime Event Implementations
        CreateClassMethodMimeEvent::Reflect(context);
        CreateGlobalMethodMimeEvent::Reflect(context);
        CreateNodeGroupMimeEvent::Reflect(context);
        CreateCommentNodeMimeEvent::Reflect(context);
        CreateCustomNodeMimeEvent::Reflect(context);
        CreateEBusHandlerMimeEvent::Reflect(context);
        CreateEBusHandlerEventMimeEvent::Reflect(context);
        CreateEBusSenderMimeEvent::Reflect(context);
        CreateGetVariableNodeMimeEvent::Reflect(context);
        CreateSetVariableNodeMimeEvent::Reflect(context);
        CreateVariableChangedNodeMimeEvent::Reflect(context);
        CreateVariableSpecificNodeMimeEvent::Reflect(context);
        CreateFunctionMimeEvent::Reflect(context);

        // Script Events
        CreateScriptEventsHandlerMimeEvent::Reflect(context);
        CreateScriptEventsReceiverMimeEvent::Reflect(context);
        CreateScriptEventsSenderMimeEvent::Reflect(context);
        CreateSendOrReceiveScriptEventsMimeEvent::Reflect(context);
        
        if (auto serialize = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serialize->Class<ReflectComponent, AZ::Component>()
                ->Version(0)
                ->Attribute(AZ::Edit::Attributes::SystemComponentTags, AZStd::vector<AZ::Crc32>({ AssetBuilderSDK::ComponentTags::AssetBuilder }));
                ;

            if (AZ::EditContext* ec = serialize->GetEditContext())
            {
                ec->Class<ReflectComponent>("Script Canvas Reflections", "Script Canvas Reflect Component")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::Category, "Scripting")
                    ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC("System"))
                    ;
            }
        }
    }

    void ReflectComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC("ScriptCanvasReflectService", 0xb3bfe139));
    }

    void ReflectComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC("ScriptCanvasReflectService", 0xb3bfe139));
    }

    void ReflectComponent::GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        required.push_back(AZ_CRC("ScriptCanvasService", 0x41fd58f3));
    }

    void ReflectComponent::Activate()
    {
    }

    void ReflectComponent::Deactivate()
    {
    }
}
