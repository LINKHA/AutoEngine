#pragma once

// Convenience header file for including commonly needed engine classes. Note: intentionally does not include Debug/DebugNew.h
// so that placement new works as expected.

#include "Container/HashSet.h"
#include "Container/List.h"
#include "Container/Ptr.h"
#include "Core/ProcessUtils.h"
#include "Core/Serializable.h"
#include "Debug/Log.h"
#include "Debug/Profiler.h"
#include "Graphics/ConstantBuffer.h"
#include "Graphics/Graphics.h"
#include "Graphics/IndexBuffer.h"
#include "Graphics/Shader.h"
#include "Graphics/ShaderVariation.h"
#include "Graphics/Texture.h"
#include "Graphics/VertexBuffer.h"
#include "IO/File.h"
#include "IO/FileSystem.h"
#include "IO/MemoryBuffer.h"
#include "IO/VectorBuffer.h"
#include "Math/Frustum.h"
#include "Math/Polyhedron.h"
#include "Math/Random.h"
#include "Math/Ray.h"
#include "Engine/Components/Camera.h"
#include "Engine/Components/Light.h"
#include "Engine/Components/Material.h"
#include "Engine/Components/Model.h"
#include "Renderer/Octree.h"
#include "Renderer/Renderer.h"
#include "Engine/Components/StaticModel.h"
#include "Engine/Components/DynamicModel.h"
#include "Resource/Image.h"
#include "Resource/JSONFile.h"
#include "Resource/ResourceCache.h"
#include "Resource/Font.h"
#include "Engine/Components/SkyBox.h"
#include "Engine/Components/Water.h"
#include "Engine/Components/IBLMaterial.h"
#include "Scene/Scene.h"
#include "RegisteredBox/RegisteredBox.h"
#include "Thread/Condition.h"
#include "Thread/Mutex.h"
#include "Thread/Thread.h"
#include "Time/Time.h"
#include "Platform/Input.h"
#include "Platform/Window.h"
#include "Audio/Audio.h"
#include "Audio/Sound.h"
#include "Audio/AudioListener.h"
#include "Audio/AudioSource.h"
#include "Audio/AudioNode.h"
#include "Script/Script.h"
#include "Auto2D/Renderer2D.h"
#include "Auto2D/Batch2D.h"
#include "Auto2D/Camera2D.h"
#include "Auto2D/Scene2D.h"
#include "Auto2D/Sprite2D.h"
#include "Auto2D/PhysicsWorld2D.h"
#include "Auto2D/RigidBody2D.h"
#include "Auto2D/Collider2D.h"
#include "Auto2D/ColliderBox2D.h"
#include "Auto2D/ColliderCircle2D.h"
#include "UI/UI.h"
#include "UI/AppLog.h"
#include "Physics/Physics.h"
#include "Physics/PhysicsWorld.h"
#include "Physics/PhysicsUtils.h"
#include "Physics/RigidBody.h"
#include "Physics/Collider.h"
#include "Physics/ColliderBox.h"
#include "Physics/ColliderSphere.h"
#include "Core/Modules/ModuleManager.h"

#include "Adapter/AutoFileDialog.h"

#include "Application.h"