class RenderFeature
{
    public:
        RenderFeature(){}
        virtual ~RenderFeature() {}
        virtual void RecreateResources(){}

        //Original:
        //virtual void RenderFrame(const legit::InFlightQueue::FrameInfo &frameInfo, const Camera &camera, const Camera &light, Scene *scene, GLFWwindow *window){}
        virtual void RenderFrame(){}


        //callback used when there are viewport resizes
        virtual void ViewportUpdate(int vpWidth, int vpHeight){}
};