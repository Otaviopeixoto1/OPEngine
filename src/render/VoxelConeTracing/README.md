# Voxel Cone Tracing

<p>
Radiance Cascades is a novel approach to both 2D and 3D global illumination created by Alexander Sannikov. It is built to produce a noiseless result by sampling and interpolating incoming radiance from a hierarchy of probes placed on the scene. Each hierarchy level/cascade samples the scene with a higher angular frequency (more rays per probe) and reduced spatial frequency (less probes overall). This project uses my custom OpenGL engine (OPEngine) to implement this technique in 2D in order to explore its intricacies.</p>

<p>
Check out <a href="https://otaviopeixoto1.github.io/portfolio/2DRadianceCascades/">my page</a> containing some of the explanation about the method. Also please refer to the demos and articles about the subject:</p>

 <ul>
  <li><a href="https://tmpvar.com/poc/radiance-cascades/">tmpvar’s playground</a></li>
  <li><a href="https://www.shadertoy.com/view/mtlBzX">Fad's shadertoy implementation</a></li>
  <li><a href="https://www.shadertoy.com/view/mlSfRD">Suslik's shadertoy implementation (fork of Fad's work)</a></li>
  <li><a href="https://drive.google.com/file/d/1L6v1_7HY2X-LV3Ofb6oyTIxgEaP4LOI6/view">Original paper by Alexander Sannikov</a></li>
</ul> 

