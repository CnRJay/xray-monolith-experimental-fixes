function normal(shader, t_base, t_second, t_detail)
	shader:begin("stub_notransform_t", "distortion_pass_r4")
	: zb(false, false)
	shader:dx10texture("s_image", "$user$generic0")
	shader:dx10texture("s_distort", "$user$generic1")
	shader:dx10sampler("smp_base")
	shader:dx10sampler("smp_rtlinear");
end