vec3 ToneMapping(vec3 color)
{
	color = clamp(u_Exposure * color, 0., 1.);

	return linearTosRGB(color);
}
