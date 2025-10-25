#pragma once

#include "pch.h"
#include "d3d12.h"
#include "openxr.h"
#include "swapchain.h"
#include "texture.h"

class SharedTexture;

class RND_Renderer {
public:
    explicit RND_Renderer(XrSession xrSession);
    ~RND_Renderer();

    void StartFrame();
    void EndFrame();
    std::optional<std::array<XrView, 2>> UpdateViews(XrTime predictedDisplayTime);
    std::optional<std::array<XrView, 2>> GetPoses() const { return m_currViews; }
    std::optional<XrFovf> GetFOV(OpenXR::EyeSide side) const { return m_currViews.transform([side](auto& views) { return views[side].fov; }); }
    std::optional<XrPosef> GetPose(OpenXR::EyeSide side) const { return m_currViews.transform([side](auto& views) { return views[side].pose; }); }
    std::optional<glm::fmat4> GetPoseAsMatrix(OpenXR::EyeSide side) const {
        return m_currViews.transform([side](auto& views) {
            const XrPosef& pose = views[side].pose;
            return ToMat4(ToGLM(pose.position), ToGLM(pose.orientation));
        });
    };
    std::optional<glm::fmat4> GetMiddlePose() const {
        if (!m_currViews.has_value()) return std::nullopt;
        const XrPosef& leftPose = m_currViews->at(OpenXR::EyeSide::LEFT).pose;
        const XrPosef& rightPose = m_currViews->at(OpenXR::EyeSide::RIGHT).pose;
        glm::fvec3 middlePos = (ToGLM(leftPose.position) + ToGLM(rightPose.position)) * 0.5f;
        glm::quat middleOri = glm::slerp(ToGLM(leftPose.orientation), ToGLM(rightPose.orientation), 0.5f);

        return ToMat4(middlePos, middleOri);
    };

    class Layer3D {
    public:
        explicit Layer3D(VkExtent2D extent);
        ~Layer3D();

        SharedTexture* CopyColorToLayer(OpenXR::EyeSide side, VkCommandBuffer copyCmdBuffer, VkImage image);
        SharedTexture* CopyDepthToLayer(OpenXR::EyeSide side, VkCommandBuffer copyCmdBuffer, VkImage image);
        bool HasCopied(OpenXR::EyeSide side) const { return m_copiedColor[side] && m_copiedDepth[side]; };
        bool HasCopiedColor(OpenXR::EyeSide side) const { return m_copiedColor[side]; };
        bool HasCopiedDepth(OpenXR::EyeSide side) const { return m_copiedDepth[side]; };
        void PrepareRendering(OpenXR::EyeSide side);
        void StartRendering();
        void Render(OpenXR::EyeSide side);
        const std::array<XrCompositionLayerProjectionView, 2>& FinishRendering();

        float GetAspectRatio(OpenXR::EyeSide side) const { return m_swapchains[side]->GetWidth() / (float)m_swapchains[side]->GetHeight(); }

    private:
        std::array<std::unique_ptr<Swapchain<DXGI_FORMAT_R8G8B8A8_UNORM_SRGB>>, 2> m_swapchains;
        std::array<std::unique_ptr<Swapchain<DXGI_FORMAT_D32_FLOAT>>, 2> m_depthSwapchains;
        std::array<std::unique_ptr<RND_D3D12::PresentPipeline<true>>, 2> m_presentPipelines;
        std::array<std::unique_ptr<SharedTexture>, 2> m_textures;
        std::array<std::unique_ptr<SharedTexture>, 2> m_depthTextures;

        std::array<XrCompositionLayerProjectionView, 2> m_projectionViews = {};
        std::array<XrCompositionLayerDepthInfoKHR, 2> m_projectionViewsDepthInfo = {};

        std::array<std::atomic_bool, 2> m_copiedColor = { false, false };
        std::array<std::atomic_bool, 2> m_copiedDepth = { false, false };
    };

    class Layer2D {
    public:
        explicit Layer2D(VkExtent2D extent);
        ~Layer2D();

        SharedTexture* CopyColorToLayer(VkCommandBuffer copyCmdBuffer, VkImage image);
        bool HasRecordedCopy() const { return m_recordedCopy; }
        bool HasCopied() const { return m_recordedCopy && m_texture->GetLastSignalledValue() == SEMAPHORE_TO_D3D12; };
        void StartRendering() const;
        void Render();
        std::vector<XrCompositionLayerQuad> FinishRendering(XrTime predictedDisplayTime);

    private:
        std::unique_ptr<Swapchain<DXGI_FORMAT_R8G8B8A8_UNORM_SRGB>> m_swapchain;
        std::unique_ptr<RND_D3D12::PresentPipeline<false>> m_presentPipeline;
        std::unique_ptr<SharedTexture> m_texture;
        std::atomic_bool m_recordedCopy = false;

        static constexpr float DISTANCE = 2.0f;
        static constexpr float LERP_SPEED = 0.05f;
        glm::quat m_currentOrientation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    };

    std::unique_ptr<Layer3D> m_layer3D;
    std::unique_ptr<Layer2D> m_layer2D;

    bool IsRendering3D() {
        return m_presented3DLastFrame;
    }
    bool IsRendering2D() {
        return m_presented2DLastFrame;
    }
    bool IsInitialized() {
        return m_isInitialized;
    }

protected:
    XrSession m_session;
    XrFrameState m_frameState = { XR_TYPE_FRAME_STATE };
    std::optional<std::array<XrView, 2>> m_currViews;

    std::atomic_bool m_isInitialized = false;
    std::atomic_bool m_presented3DLastFrame = false;
    std::atomic_bool m_presented2DLastFrame = false;
};
