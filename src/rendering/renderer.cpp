#include "renderer.h"
#include "instance.h"
#include "texture.h"


RND_Renderer::RND_Renderer(XrSession xrSession): m_session(xrSession) {
    XrSessionBeginInfo m_sessionCreateInfo = { XR_TYPE_SESSION_BEGIN_INFO };
    m_sessionCreateInfo.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
    checkXRResult(xrBeginSession(VRManager::instance().XR->GetSession(), &m_sessionCreateInfo), "Failed to begin OpenXR session!");
}

RND_Renderer::~RND_Renderer() {
    checkXRResult(xrEndSession(VRManager::instance().XR->GetSession()), "Failed to end OpenXR session!");
}

void RND_Renderer::StartFrame() {
    XrFrameWaitInfo waitFrameInfo = { XR_TYPE_FRAME_WAIT_INFO };
    checkXRResult(xrWaitFrame(m_session, &waitFrameInfo, &m_frameState), "Failed to wait for next frame!");

    XrFrameBeginInfo beginFrameInfo = { XR_TYPE_FRAME_BEGIN_INFO };
    checkXRResult(xrBeginFrame(m_session, &beginFrameInfo), "Couldn't begin OpenXR frame!");

    VRManager::instance().D3D12->StartFrame();

    VRManager::instance().XR->UpdateTime(VRManager::instance().Hooks->s_eyeSide, m_frameState.predictedDisplayTime);

    // only update one at a time to do alternating eye rendering
    VRManager::instance().XR->UpdatePoses(OpenXR::EyeSide::LEFT);
    VRManager::instance().XR->UpdatePoses(OpenXR::EyeSide::RIGHT);

    if (m_layer3D.GetStatus() != Layer::Status::PREPARING)
        m_layer3D.PrepareRendering();
    if (m_layerHUD.GetStatus() != Layer::Status::PREPARING)
        m_layerHUD.PrepareRendering();
    if (m_layerInventory.GetStatus() != Layer::Status::PREPARING)
        m_layerInventory.PrepareRendering();
}

void RND_Renderer::EndFrame() {
    if (m_layer3D.GetStatus() == Layer::Status::BINDING)
        m_layer3D.StartRendering();
    if (m_layerHUD.GetStatus() == Layer::Status::BINDING)
        m_layerHUD.StartRendering();
    if (m_layerInventory.GetStatus() == Layer::Status::BINDING)
        m_layerInventory.StartRendering();

    std::vector<XrCompositionLayerBaseHeader*> compositionLayers;

    XrCompositionLayerProjection layer3D = { XR_TYPE_COMPOSITION_LAYER_PROJECTION };
    std::array<XrCompositionLayerProjectionView, 2> layer3DViews = { XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW };
    if (m_frameState.shouldRender && m_layer3D.GetStatus() == Layer::Status::RENDERING) {
        m_layer3D.Render(OpenXR::EyeSide::LEFT);
        m_layer3D.Render(OpenXR::EyeSide::RIGHT);
        layer3DViews = m_layer3D.FinishRendering();
        layer3D.layerFlags = NULL;
        layer3D.space = VRManager::instance().XR->m_stageSpace;
        layer3D.viewCount = (uint32_t)layer3DViews.size();
        layer3D.views = layer3DViews.data();
        compositionLayers.emplace_back(reinterpret_cast<XrCompositionLayerBaseHeader*>(&layer3D));
    }

    XrCompositionLayerQuad layerHUD = { XR_TYPE_COMPOSITION_LAYER_QUAD };
    if (m_frameState.shouldRender && m_layerHUD.GetStatus() == Layer::Status::RENDERING) {
        m_layerHUD.Render();
        layerHUD = m_layerHUD.FinishRendering();
        compositionLayers.emplace_back(reinterpret_cast<XrCompositionLayerBaseHeader*>(&layerHUD));
    }

    XrCompositionLayerQuad layerInventory = { XR_TYPE_COMPOSITION_LAYER_QUAD };
    if (m_frameState.shouldRender && m_layerInventory.GetStatus() == Layer::Status::RENDERING) {
        m_layerInventory.Render();
        layerInventory = m_layerInventory.FinishRendering();
        compositionLayers.emplace_back(reinterpret_cast<XrCompositionLayerBaseHeader*>(&layerInventory));
    }

    XrFrameEndInfo frameEndInfo = { XR_TYPE_FRAME_END_INFO };
    frameEndInfo.displayTime = m_frameState.predictedDisplayTime;
    frameEndInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
    frameEndInfo.layerCount = (uint32_t)compositionLayers.size();
    frameEndInfo.layers = compositionLayers.data();
    checkXRResult(xrEndFrame(m_session, &frameEndInfo), "Failed to render texture!");

    VRManager::instance().D3D12->EndFrame();
}

RND_Renderer::Layer3D::Layer3D(): Layer() {
    auto viewConfs = VRManager::instance().XR->GetViewConfigurations();

    this->m_presentPipelines[OpenXR::EyeSide::LEFT] = std::make_unique<RND_D3D12::PresentPipeline<true>>(VRManager::instance().XR->GetRenderer());
    this->m_presentPipelines[OpenXR::EyeSide::RIGHT] = std::make_unique<RND_D3D12::PresentPipeline<true>>(VRManager::instance().XR->GetRenderer());

    // note: it's possible to make a swapchain that matches Cemu's internal resolution and let the headset downsample it, although I doubt there's a benefit
    this->m_swapchains[OpenXR::EyeSide::LEFT] = std::make_unique<Swapchain<DXGI_FORMAT_R8G8B8A8_UNORM_SRGB>>(viewConfs[0].recommendedImageRectWidth, viewConfs[0].recommendedImageRectHeight, viewConfs[0].recommendedSwapchainSampleCount);
    this->m_swapchains[OpenXR::EyeSide::RIGHT] = std::make_unique<Swapchain<DXGI_FORMAT_R8G8B8A8_UNORM_SRGB>>(viewConfs[1].recommendedImageRectWidth, viewConfs[1].recommendedImageRectHeight, viewConfs[1].recommendedSwapchainSampleCount);
    this->m_depthSwapchains[OpenXR::EyeSide::LEFT] = std::make_unique<Swapchain<DXGI_FORMAT_D32_FLOAT>>(viewConfs[0].recommendedImageRectWidth, viewConfs[0].recommendedImageRectHeight, viewConfs[0].recommendedSwapchainSampleCount);
    this->m_depthSwapchains[OpenXR::EyeSide::RIGHT] = std::make_unique<Swapchain<DXGI_FORMAT_D32_FLOAT>>(viewConfs[1].recommendedImageRectWidth, viewConfs[1].recommendedImageRectHeight, viewConfs[1].recommendedSwapchainSampleCount);

    this->m_presentPipelines[OpenXR::EyeSide::LEFT]->BindSettings((float)this->m_swapchains[OpenXR::EyeSide::LEFT]->GetWidth(), (float)this->m_swapchains[OpenXR::EyeSide::LEFT]->GetHeight());
    this->m_presentPipelines[OpenXR::EyeSide::RIGHT]->BindSettings((float)this->m_swapchains[OpenXR::EyeSide::RIGHT]->GetWidth(), (float)this->m_swapchains[OpenXR::EyeSide::RIGHT]->GetHeight());
}

void RND_Renderer::Layer3D::PrepareRendering() {
    checkAssert(m_status == Status::NOT_RENDERING, "Need to finish rendering the previous frame before starting a new one");
    m_status = Status::PREPARING;

    this->m_textures[OpenXR::EyeSide::LEFT] = nullptr;
    this->m_textures[OpenXR::EyeSide::RIGHT] = nullptr;

    this->m_swapchains[OpenXR::EyeSide::LEFT]->PrepareRendering();
    this->m_swapchains[OpenXR::EyeSide::RIGHT]->PrepareRendering();
    this->m_depthSwapchains[OpenXR::EyeSide::LEFT]->PrepareRendering();
    this->m_depthSwapchains[OpenXR::EyeSide::RIGHT]->PrepareRendering();
}

void RND_Renderer::Layer3D::StartRendering() {
    checkAssert(m_status == Status::BINDING, "Haven't attached any textures to the layer yet so there's nothing to start rendering");
    m_status = Status::RENDERING;

    checkAssert((this->m_textures[OpenXR::EyeSide::LEFT] == nullptr && this->m_textures[OpenXR::EyeSide::RIGHT] == nullptr) || (this->m_textures[OpenXR::EyeSide::LEFT] != nullptr && this->m_textures[OpenXR::EyeSide::RIGHT] != nullptr), "Both textures must be either null or not null");
    checkAssert((this->m_depthTextures[OpenXR::EyeSide::LEFT] == nullptr && this->m_depthTextures[OpenXR::EyeSide::RIGHT] == nullptr) || (this->m_depthTextures[OpenXR::EyeSide::LEFT] != nullptr && this->m_depthTextures[OpenXR::EyeSide::RIGHT] != nullptr), "Both depth textures must be either null or not null");

    this->m_swapchains[OpenXR::EyeSide::LEFT]->StartRendering();
    this->m_swapchains[OpenXR::EyeSide::RIGHT]->StartRendering();
    this->m_depthSwapchains[OpenXR::EyeSide::LEFT]->StartRendering();
    this->m_depthSwapchains[OpenXR::EyeSide::RIGHT]->StartRendering();
}

void RND_Renderer::Layer3D::Render(OpenXR::EyeSide side) {
    ID3D12Device* device = VRManager::instance().D3D12->GetDevice();
    ID3D12CommandQueue* queue = VRManager::instance().D3D12->GetCommandQueue();
    ID3D12CommandAllocator* allocator = VRManager::instance().D3D12->GetFrameAllocator();
    RND_D3D12::CommandContext<false> renderSharedTexture(device, queue, allocator, [this, side](ID3D12GraphicsCommandList* cmdList) {
        cmdList->SetName(L"RenderSharedTexture");
        m_textures[side]->d3d12WaitForFence(1);
        m_textures[side]->d3d12TransitionLayout(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        m_depthTextures[side]->d3d12WaitForFence(1);
        m_depthTextures[side]->d3d12TransitionLayout(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

        m_presentPipelines[side]->BindAttachment(0, m_textures[side]->d3d12GetTexture());
        m_presentPipelines[side]->BindAttachment(1, m_depthTextures[side]->d3d12GetTexture(), DXGI_FORMAT_R32_FLOAT);
        m_presentPipelines[side]->BindTarget(0, m_swapchains[side]->GetTexture(), m_swapchains[side]->GetFormat());
        m_presentPipelines[side]->BindDepthTarget(m_depthSwapchains[side]->GetTexture(), m_depthSwapchains[side]->GetFormat());
        m_presentPipelines[side]->Render(cmdList, m_swapchains[side]->GetTexture());

        m_depthTextures[side]->d3d12TransitionLayout(cmdList, D3D12_RESOURCE_STATE_COPY_DEST);
        m_depthTextures[side]->d3d12SignalFence(0);
        m_textures[side]->d3d12TransitionLayout(cmdList, D3D12_RESOURCE_STATE_COPY_DEST);
        m_textures[side]->d3d12SignalFence(0);
    });
}

const std::array<XrCompositionLayerProjectionView, 2>& RND_Renderer::Layer3D::FinishRendering() {
    checkAssert(m_status == Status::RENDERING, "Should have rendered before ending it");
    m_status = Status::NOT_RENDERING;

    this->m_swapchains[OpenXR::EyeSide::LEFT]->FinishRendering();
    this->m_swapchains[OpenXR::EyeSide::RIGHT]->FinishRendering();
    this->m_depthSwapchains[OpenXR::EyeSide::LEFT]->FinishRendering();
    this->m_depthSwapchains[OpenXR::EyeSide::RIGHT]->FinishRendering();

    XrView leftView = VRManager::instance().XR->GetPredictedView(OpenXR::EyeSide::LEFT);
    XrView rightView = VRManager::instance().XR->GetPredictedView(OpenXR::EyeSide::RIGHT);

    m_projectionViews[OpenXR::EyeSide::LEFT] = {
        .type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW,
        .next = &m_projectionViewsDepthInfo[OpenXR::EyeSide::LEFT],
        .pose = leftView.pose,
        .fov = leftView.fov,
        .subImage = {
            .swapchain = this->m_swapchains[OpenXR::EyeSide::LEFT]->GetHandle(),
            .imageRect = {
                .offset = { 0, 0 },
                .extent = {
                    .width = (int32_t)this->m_swapchains[OpenXR::EyeSide::LEFT]->GetWidth(),
                    .height = (int32_t)this->m_swapchains[OpenXR::EyeSide::LEFT]->GetHeight()
                }
            }
        },
    };
    m_projectionViewsDepthInfo[OpenXR::EyeSide::LEFT] = {
        .type = XR_TYPE_COMPOSITION_LAYER_DEPTH_INFO_KHR,
        .subImage = {
            .swapchain = this->m_depthSwapchains[OpenXR::EyeSide::LEFT]->GetHandle(),
            .imageRect = {
                .offset = { 0, 0 },
                .extent = {
                    .width = (int32_t)this->m_depthSwapchains[OpenXR::EyeSide::LEFT]->GetWidth(),
                    .height = (int32_t)this->m_depthSwapchains[OpenXR::EyeSide::LEFT]->GetHeight()
                }
            },
        },
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
        .nearZ = 0.1f,
        .farZ = 1000.0f,
    };
    m_projectionViews[OpenXR::EyeSide::RIGHT] = {
        .type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW,
        .next = &m_projectionViewsDepthInfo[OpenXR::EyeSide::RIGHT],
        .pose = rightView.pose,
        .fov = rightView.fov,
        .subImage = {
            .swapchain = this->m_swapchains[OpenXR::EyeSide::RIGHT]->GetHandle(),
            .imageRect = {
                .offset = { 0, 0 },
                .extent = {
                    .width = (int32_t)this->m_swapchains[OpenXR::EyeSide::RIGHT]->GetWidth(),
                    .height = (int32_t)this->m_swapchains[OpenXR::EyeSide::RIGHT]->GetHeight()
                }
            }
        },
    };
    m_projectionViewsDepthInfo[OpenXR::EyeSide::RIGHT] = {
        .type = XR_TYPE_COMPOSITION_LAYER_DEPTH_INFO_KHR,
        .subImage = {
            .swapchain = this->m_depthSwapchains[OpenXR::EyeSide::RIGHT]->GetHandle(),
            .imageRect = {
                .offset = { 0, 0 },
                .extent = {
                    .width = (int32_t)this->m_depthSwapchains[OpenXR::EyeSide::RIGHT]->GetWidth(),
                    .height = (int32_t)this->m_depthSwapchains[OpenXR::EyeSide::RIGHT]->GetHeight()
                }
            },
        },
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
        .nearZ = 0.1f,
        .farZ = 1000.0f,
    };
    return m_projectionViews;
}

RND_Renderer::Layer2D::Layer2D(): Layer() {
    auto viewConfs = VRManager::instance().XR->GetViewConfigurations();

    this->m_presentPipeline = std::make_unique<RND_D3D12::PresentPipeline<false>>(VRManager::instance().XR->GetRenderer());

    // note: it's possible to make a swapchain that matches Cemu's internal resolution and let the headset downsample it, although I doubt there's a benefit
    this->m_swapchain = std::make_unique<Swapchain<DXGI_FORMAT_R8G8B8A8_UNORM_SRGB>>(viewConfs[0].recommendedImageRectWidth, viewConfs[0].recommendedImageRectHeight, viewConfs[0].recommendedSwapchainSampleCount);

    this->m_presentPipeline->BindSettings((float)this->m_swapchain->GetWidth(), (float)this->m_swapchain->GetHeight());
}

void RND_Renderer::Layer2D::PrepareRendering() {
    checkAssert(m_status == Status::NOT_RENDERING, "Need to finish rendering the previous frame before starting a new one");
    m_status = Status::PREPARING;

    this->m_texture = nullptr;

    this->m_swapchain->PrepareRendering();
}

void RND_Renderer::Layer2D::StartRendering() {
    checkAssert(m_status == Status::BINDING, "Haven't attached any textures to the layer yet so there's nothing to start rendering");
    m_status = Status::RENDERING;

    checkAssert(this->m_texture == nullptr, "Shouldn't start rendering when there's no texture to render to this layer!");

    this->m_swapchain->StartRendering();
}

void RND_Renderer::Layer2D::Render() {
    ID3D12Device* device = VRManager::instance().D3D12->GetDevice();
    ID3D12CommandQueue* queue = VRManager::instance().D3D12->GetCommandQueue();
    ID3D12CommandAllocator* allocator = VRManager::instance().D3D12->GetFrameAllocator();
    RND_D3D12::CommandContext<false> renderSharedTexture(device, queue, allocator, [this](ID3D12GraphicsCommandList* cmdList) {
        cmdList->SetName(L"RenderSharedTexture");
        m_texture->d3d12WaitForFence(1);
        m_texture->d3d12TransitionLayout(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

        m_presentPipeline->BindAttachment(0, m_texture->d3d12GetTexture());
        m_presentPipeline->BindTarget(0, m_swapchain->GetTexture(), m_swapchain->GetFormat());
        m_presentPipeline->Render(cmdList, m_swapchain->GetTexture());

        m_texture->d3d12TransitionLayout(cmdList, D3D12_RESOURCE_STATE_COPY_DEST);
        m_texture->d3d12SignalFence(0);
    });
}

XrCompositionLayerQuad RND_Renderer::Layer2D::FinishRendering() {
    checkAssert(m_status == Status::RENDERING, "Should have rendered before ending it");
    m_status = Status::NOT_RENDERING;

    this->m_swapchain->FinishRendering();

    XrView leftView = VRManager::instance().XR->GetPredictedView(OpenXR::EyeSide::LEFT);
    XrView rightView = VRManager::instance().XR->GetPredictedView(OpenXR::EyeSide::RIGHT);

    return {
        .type = XR_TYPE_COMPOSITION_LAYER_QUAD,
        .layerFlags = NULL,
        .space = VRManager::instance().XR->m_stageSpace,
        .eyeVisibility = XR_EYE_VISIBILITY_BOTH,
        .subImage = {
            .swapchain = this->m_swapchain->GetHandle(),
            .imageRect = {
                .offset = { 0, 0 },
                .extent = {
                    .width = (int32_t)this->m_swapchain->GetWidth(),
                    .height = (int32_t)this->m_swapchain->GetHeight()
                }
            }
        },
        .pose = {
            .orientation = { 0, 0, 0, 1 },
            .position = { 0, 0, 0 }
        },
        .size = { 1, 1 }
    };
}
