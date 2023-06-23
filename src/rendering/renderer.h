#pragma once

#include "d3d12.h"
#include "openxr.h"


class RND_Renderer {
public:
    explicit RND_Renderer(XrSession xrSession);
    ~RND_Renderer();

    void StartFrame();
    void EndFrame();

protected:
    XrSession m_session;

    enum class LayerType : uint8_t {
        LAYER_3D = 0,
        LAYER_2D = 1,
    };

    class Layer {
    public:
        Layer() = default;
        virtual ~Layer() = default;

        virtual void PrepareRendering() = 0;
        virtual void StartRendering() = 0;

        enum class Status {
            NOT_RENDERING,
            PREPARING,
            BINDING,
            RENDERING,
        };
        [[nodiscard]] Status GetStatus() const { return m_status; }
    protected:
        Status m_status = Status::NOT_RENDERING;
    };

    class Layer3D : public Layer {
    public:
        Layer3D();

        ~Layer3D() override {
            for (auto& swapchain : m_swapchains) {
                swapchain.reset();
            }
        };

        void PrepareRendering() override;
        void AddTexture(OpenXR::EyeSide side, class SharedTexture* texture) {
            checkAssert(m_status == Status::PREPARING || m_status == Status::BINDING, "Need to prepare the layer before adding textures to it");
            m_status = Status::BINDING;

            this->m_textures[side] = texture;
        };
        void AddDepthTexture(OpenXR::EyeSide side, class SharedTexture* depthTexture) {
            checkAssert(m_status == Status::PREPARING || m_status == Status::BINDING, "Need to prepare the layer before adding textures to it");
            m_status = Status::BINDING;

            this->m_depthTextures[side] = depthTexture;
        };
        void StartRendering() override;
        void Render(OpenXR::EyeSide side);
        const std::array<XrCompositionLayerProjectionView, 2>& FinishRendering();
    private:
        std::array<std::unique_ptr<Swapchain<DXGI_FORMAT_R8G8B8A8_UNORM_SRGB>>, 2> m_swapchains;
        std::array<std::unique_ptr<Swapchain<DXGI_FORMAT_D32_FLOAT>>, 2> m_depthSwapchains;
        std::array<std::unique_ptr<RND_D3D12::PresentPipeline<true>>, 2> m_presentPipelines;
        std::array<class SharedTexture*, 2> m_textures = { nullptr, nullptr };
        std::array<class SharedTexture*, 2> m_depthTextures = { nullptr, nullptr };
        std::array<XrCompositionLayerProjectionView, 2> m_projectionViews = {};
        std::array<XrCompositionLayerDepthInfoKHR, 2> m_projectionViewsDepthInfo = {};
    };

    class Layer2D : public Layer {
    public:
        Layer2D();

        ~Layer2D() override {
            m_swapchain.reset();
        };

        void PrepareRendering() override;
        void AddTexture(class SharedTexture* texture) {
            checkAssert(m_status == Status::PREPARING, "Need to prepare the layer before adding textures to it");
            m_status = Status::BINDING;

            this->m_texture = texture;
        };
        void StartRendering() override;
        void Render();
        XrCompositionLayerQuad FinishRendering();
    private:
        std::unique_ptr<Swapchain<DXGI_FORMAT_R8G8B8A8_UNORM_SRGB>> m_swapchain;
        std::unique_ptr<RND_D3D12::PresentPipeline<false>> m_presentPipeline;
        class SharedTexture* m_texture = nullptr;
    };

public:
    Layer3D m_layer3D;
    Layer2D m_layerHUD;
    Layer2D m_layerInventory;

protected:
    XrFrameState m_frameState = { XR_TYPE_FRAME_STATE };
};
