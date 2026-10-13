struct type_13 {
    member: vec3<f32>,
    member_1: f32,
    member_2: vec3<f32>,
    member_3: f32,
    member_4: vec3<f32>,
    member_5: f32,
    member_6: vec3<f32>,
    member_7: f32,
    member_8: vec3<f32>,
    member_9: f32,
    member_10: vec3<f32>,
    member_11: f32,
    member_12: f32,
    member_13: f32,
    member_14: f32,
    member_15: f32,
    member_16: vec3<f32>,
    member_17: f32,
    member_18: f32,
    member_19: f32,
    member_20: f32,
    member_21: f32,
    member_22: vec2<f32>,
    member_23: f32,
    member_24: f32,
    member_25: vec4<f32>,
    member_26: vec4<f32>,
    member_27: vec4<f32>,
    member_28: vec4<f32>,
    member_29: vec4<f32>,
    member_30: vec4<f32>,
    member_31: vec4<f32>,
    member_32: vec4<f32>,
    member_33: vec4<f32>,
    member_34: vec4<f32>,
}

struct type_16 {
    member: f32,
    member_1: f32,
    member_2: f32,
    member_3: f32,
    member_4: vec3<f32>,
    member_5: f32,
    member_6: vec3<f32>,
    member_7: f32,
    member_8: vec3<f32>,
    member_9: f32,
    member_10: vec3<f32>,
    member_11: vec3<f32>,
    member_12: vec3<f32>,
    member_13: vec3<f32>,
    member_14: f32,
    member_15: f32,
    member_16: f32,
    member_17: f32,
    member_18: f32,
    member_19: vec2<f32>,
    member_20: f32,
    member_21: f32,
    member_22: f32,
    member_23: vec3<f32>,
    member_24: u32,
    member_25: f32,
    member_26: vec3<f32>,
    member_27: f32,
    member_28: vec3<f32>,
    member_29: f32,
}

@group(0) @binding(0)
var<uniform> global: type_13;
@group(0) @binding(2)
var<uniform> global_1: type_16;
@group(0) @binding(67)
var global_2: sampler;
@group(0) @binding(3)
var global_3: texture_2d<f32>;
@group(0) @binding(65)
var global_4: sampler;
@group(0) @binding(1)
var global_5: texture_2d<f32>;
var<private> global_6: vec4<f32>;
@group(0) @binding(68)
var global_7: sampler;
@group(0) @binding(4)
var global_8: texture_2d<f32>;
var<private> global_9: vec2<f32>;

fn function_() {
    var local: array<vec2<f32>, 4>;
    var local_1: array<vec2<f32>, 4>;
    var local_2: array<vec2<f32>, 4>;
    var local_3: array<vec2<f32>, 4>;
    var phi_7750_: vec3<f32>;
    var phi_7682_: vec3<f32>;
    var phi_7683_: vec3<f32>;
    var phi_7684_: vec3<f32>;
    var phi_7688_: vec3<f32>;
    var phi_7686_: f32;
    var phi_7689_: vec3<f32>;
    var phi_7690_: vec3<f32>;
    var local_4: bool;
    var local_5: f32;
    var phi_7699_: vec3<f32>;
    var phi_7697_: f32;
    var phi_7700_: vec3<f32>;
    var phi_7701_: vec3<f32>;
    var local_6: bool;
    var local_7: f32;
    var phi_7705_: vec3<f32>;
    var phi_7703_: f32;
    var phi_7706_: vec3<f32>;
    var phi_7707_: vec3<f32>;
    var phi_7708_: vec3<f32>;
    var phi_3686_: bool;
    var phi_7709_: vec2<f32>;
    var phi_7714_: f32;
    var phi_7710_: f32;
    var phi_7722_: vec3<f32>;
    var phi_7723_: vec3<f32>;
    var phi_7715_: i32;
    var phi_7728_: vec2<f32>;
    var phi_7727_: vec2<f32>;
    var phi_7730_: f32;
    var phi_7729_: i32;
    var phi_7731_: f32;
    var phi_7732_: f32;
    var local_8: f32;
    var phi_7737_: f32;
    var phi_7721_: vec3<f32>;
    var phi_7717_: vec2<f32>;
    var phi_7716_: vec2<f32>;
    var phi_7718_: f32;
    var phi_7726_: vec3<f32>;
    var phi_7725_: vec3<f32>;
    var phi_7577_: f32;
    var phi_6016_: bool;
    var phi_7573_: f32;
    var phi_7574_: vec3<f32>;
    var phi_7580_: f32;
    var phi_7653_: vec3<f32>;
    var phi_7652_: vec3<f32>;
    var phi_7637_: vec3<f32>;
    var phi_7582_: i32;
    var phi_7666_: f32;
    var phi_7665_: i32;
    var phi_7667_: f32;
    var phi_7668_: f32;
    var local_9: f32;
    var phi_7673_: f32;
    var phi_7636_: vec3<f32>;
    var phi_7595_: vec3<f32>;
    var phi_7593_: f32;
    var phi_7596_: vec3<f32>;
    var phi_7598_: vec3<f32>;
    var phi_7597_: i32;
    var local_10: vec3<f32>;
    var phi_7601_: f32;
    var phi_7600_: bool;
    var phi_7627_: f32;
    var phi_7619_: vec3<f32>;
    var phi_7207_: bool;
    var phi_7620_: f32;
    var phi_7621_: vec3<f32>;
    var phi_7629_: vec3<f32>;
    var phi_7630_: vec3<f32>;
    var phi_7631_: vec3<f32>;
    var local_11: vec3<f32>;
    var local_12: f32;
    var local_13: vec3<f32>;
    var local_14: vec3<f32>;
    var local_15: f32;
    var local_16: vec3<f32>;

    switch bitcast<i32>(0u) {
        default: {
            let _e201 = global_9;
            let _e204 = ((_e201.x * 2f) - 1f);
            let _e208 = (((1f - _e201.y) * 2f) - 1f);
            let _e210 = global.member_21;
            let _e212 = global.member_20;
            let _e213 = max(_e212, 0.001f);
            if (_e210 <= 0f) {
                switch bitcast<i32>(0u) {
                    default: {
                        let _e217 = global.member_23;
                        let _e218 = clamp(_e217, 0f, 1f);
                        let _e220 = global.member_7;
                        let _e222 = (_e208 - clamp(_e220, -1f, 1f));
                        let _e225 = clamp(((1f - _e222) * 0.5f), 0f, 1f);
                        let _e228 = global.member_32[3u];
                        if (_e228 >= 0.5f) {
                            if (_e225 < 0.5f) {
                                let _e232 = global.member_32;
                                let _e235 = global.member_33;
                                phi_7750_ = mix(_e232.xyz, _e235.xyz, vec3(smoothstep(0f, 0.5f, _e225)));
                            } else {
                                let _e241 = global.member_33;
                                let _e244 = global.member_34;
                                phi_7750_ = mix(_e241.xyz, _e244.xyz, vec3(smoothstep(0.5f, 1f, _e225)));
                            }
                            let _e250 = phi_7750_;
                            let _e252 = global.member_1;
                            let _e254 = (_e250 * exp2(_e252));
                            global_6 = vec4<f32>(_e254.x, _e254.y, _e254.z, 1f);
                            break;
                        }
                        let _e260 = global_1.member;
                        let _e264 = global.member;
                        let _e266 = normalize((_e264 - vec3<f32>(0f, -(_e260), 0f)));
                        let _e268 = global.member_6;
                        let _e271 = (_e268 - (_e266 * dot(_e268, _e266)));
                        phi_7682_ = _e271;
                        if (dot(_e271, _e271) < 0.000001f) {
                            let _e275 = global.member_4;
                            phi_7682_ = (_e275 - (_e266 * dot(_e275, _e266)));
                        }
                        let _e280 = phi_7682_;
                        phi_7683_ = _e280;
                        if (dot(_e280, _e280) < 0.000001f) {
                            let _e284 = global.member_2;
                            phi_7683_ = (_e284 - (_e266 * dot(_e284, _e266)));
                        }
                        let _e289 = phi_7683_;
                        phi_7684_ = _e289;
                        if (dot(_e289, _e289) < 0.000001f) {
                            let _e296 = select(vec3<f32>(1f, 0f, 0f), vec3<f32>(0f, 0f, 1f), vec3((abs(_e266.z) < 0.98f)));
                            phi_7684_ = (_e296 - (_e266 * dot(_e296, _e266)));
                        }
                        let _e301 = phi_7684_;
                        let _e302 = normalize(_e301);
                        switch bitcast<i32>(0u) {
                            default: {
                                let _e306 = global.member_29[3u];
                                let _e307 = (_e306 >= 0f);
                                local_4 = _e307;
                                local_5 = _e306;
                                local_6 = _e307;
                                local_7 = _e306;
                                if _e307 {
                                    let _e310 = global.member_29[2u];
                                    let _e311 = sin(_e310);
                                    let _e312 = cos(_e310);
                                    let _e329 = textureSampleLevel(global_5, global_4, vec2<f32>(((atan2(((_e311 * _e266.x) + (_e312 * _e266.z)), ((_e312 * _e266.x) - (_e311 * _e266.z))) * 0.15915494f) + 0.5f), (acos(clamp(_e266.y, -1f, 1f)) * 0.31830987f)), 0f);
                                    phi_7690_ = (_e329.xyz * _e306);
                                    break;
                                }
                                let _e338 = normalize(cross(_e266, select(vec3<f32>(0f, 0f, 1f), vec3<f32>(1f, 0f, 0f), vec3((abs(_e266.z) > 0.98f)))));
                                let _e342 = dot(_e266, _e266);
                                let _e352 = ((((asin(clamp(_e342, -1f, 1f)) * 0.31830987f) + 0.5f) * 2f) - 1f);
                                let _e358 = (0.5f + ((0.5f * sign(_e352)) * sqrt(abs(_e352))));
                                let _e359 = vec2<f32>(((atan2(dot(_e266, normalize(cross(_e338, _e266))), dot(_e266, _e338)) * 0.15915494f) + 0.5f), _e358);
                                let _e364 = textureSampleLevel(global_5, global_4, vec2<f32>(_e359.x, clamp(_e358, 0.001953125f, 0.9980469f)), 0f);
                                let _e367 = global_1.member_24;
                                let _e368 = (_e367 == 2u);
                                let _e370 = global_1.member_8;
                                let _e372 = global_1.member_9;
                                let _e374 = global_1.member_10;
                                let _e376 = global_1.member_18;
                                let _e378 = global_1.member_23;
                                let _e380 = global_1.member_17;
                                let _e382 = global_1.member_13;
                                let _e386 = vec3(clamp(_e218, 0f, 1f));
                                let _e387 = mix((_e370 * max(_e372, 0f)), _e374, _e386);
                                phi_7688_ = _e364.xyz;
                                if (_e368 && (_e342 < 0f)) {
                                    phi_7688_ = mix(mix(_e387, _e378, vec3(clamp(_e376, 0f, 1f))), _e387, vec3(clamp((-(_e342) * max(_e380, 0.001f)), 0f, 1f)));
                                }
                                let _e400 = phi_7688_;
                                phi_7689_ = _e400;
                                if ((_e218 > 0f) && (_e342 > select(-1f, 0f, _e368))) {
                                    if _e368 {
                                        phi_7686_ = max(_e342, 0f);
                                    } else {
                                        phi_7686_ = abs(_e342);
                                    }
                                    let _e408 = phi_7686_;
                                    let _e420 = fract((((normalize(_e266) * 37f) * 0.3183099f) + vec3<f32>(0.1f, 0.2f, 0.3f)));
                                    let _e425 = (_e420 + vec3(dot(_e420, (_e420.yzx + vec3<f32>(19.19f, 19.19f, 19.19f)))));
                                    phi_7689_ = mix(_e400, (mix(mix(_e400, vec3<f32>(0.00018f, 0.00009f, 0.0006f), vec3(smoothstep(0f, 0.85f, _e408))), _e382, vec3((1f - smoothstep(0f, 0.55f, _e408)))) * (1f + ((fract(((_e425.x * _e425.y) * _e425.z)) - 0.5f) * 0.05f))), _e386);
                                }
                                let _e438 = phi_7689_;
                                phi_7690_ = _e438;
                                break;
                            }
                        }
                        let _e440 = phi_7690_;
                        switch bitcast<i32>(0u) {
                            default: {
                                let _e443 = local_4;
                                if _e443 {
                                    let _e446 = global.member_29[2u];
                                    let _e447 = sin(_e446);
                                    let _e448 = cos(_e446);
                                    let _e465 = textureSampleLevel(global_5, global_4, vec2<f32>(((atan2(((_e447 * _e302.x) + (_e448 * _e302.z)), ((_e448 * _e302.x) - (_e447 * _e302.z))) * 0.15915494f) + 0.5f), (acos(clamp(_e302.y, -1f, 1f)) * 0.31830987f)), 0f);
                                    let _e468 = local_5;
                                    phi_7701_ = (_e465.xyz * _e468);
                                    break;
                                }
                                let _e476 = normalize(cross(_e266, select(vec3<f32>(0f, 0f, 1f), vec3<f32>(1f, 0f, 0f), vec3((abs(_e266.z) > 0.98f)))));
                                let _e480 = dot(_e302, _e266);
                                let _e490 = ((((asin(clamp(_e480, -1f, 1f)) * 0.31830987f) + 0.5f) * 2f) - 1f);
                                let _e496 = (0.5f + ((0.5f * sign(_e490)) * sqrt(abs(_e490))));
                                let _e497 = vec2<f32>(((atan2(dot(_e302, normalize(cross(_e476, _e266))), dot(_e302, _e476)) * 0.15915494f) + 0.5f), _e496);
                                let _e502 = textureSampleLevel(global_5, global_4, vec2<f32>(_e497.x, clamp(_e496, 0.001953125f, 0.9980469f)), 0f);
                                let _e505 = global_1.member_24;
                                let _e506 = (_e505 == 2u);
                                let _e508 = global_1.member_8;
                                let _e510 = global_1.member_9;
                                let _e512 = global_1.member_10;
                                let _e514 = global_1.member_18;
                                let _e516 = global_1.member_23;
                                let _e518 = global_1.member_17;
                                let _e520 = global_1.member_13;
                                let _e524 = vec3(clamp(_e218, 0f, 1f));
                                let _e525 = mix((_e508 * max(_e510, 0f)), _e512, _e524);
                                phi_7699_ = _e502.xyz;
                                if (_e506 && (_e480 < 0f)) {
                                    phi_7699_ = mix(mix(_e525, _e516, vec3(clamp(_e514, 0f, 1f))), _e525, vec3(clamp((-(_e480) * max(_e518, 0.001f)), 0f, 1f)));
                                }
                                let _e538 = phi_7699_;
                                phi_7700_ = _e538;
                                if ((_e218 > 0f) && (_e480 > select(-1f, 0f, _e506))) {
                                    if _e506 {
                                        phi_7697_ = max(_e480, 0f);
                                    } else {
                                        phi_7697_ = abs(_e480);
                                    }
                                    let _e546 = phi_7697_;
                                    let _e558 = fract((((normalize(_e302) * 37f) * 0.3183099f) + vec3<f32>(0.1f, 0.2f, 0.3f)));
                                    let _e563 = (_e558 + vec3(dot(_e558, (_e558.yzx + vec3<f32>(19.19f, 19.19f, 19.19f)))));
                                    phi_7700_ = mix(_e538, (mix(mix(_e538, vec3<f32>(0.00018f, 0.00009f, 0.0006f), vec3(smoothstep(0f, 0.85f, _e546))), _e520, vec3((1f - smoothstep(0f, 0.55f, _e546)))) * (1f + ((fract(((_e563.x * _e563.y) * _e563.z)) - 0.5f) * 0.05f))), _e524);
                                }
                                let _e576 = phi_7700_;
                                phi_7701_ = _e576;
                                break;
                            }
                        }
                        let _e578 = phi_7701_;
                        let _e579 = -(_e266);
                        switch bitcast<i32>(0u) {
                            default: {
                                let _e582 = local_6;
                                if _e582 {
                                    let _e585 = global.member_29[2u];
                                    let _e586 = sin(_e585);
                                    let _e587 = cos(_e585);
                                    let _e604 = textureSampleLevel(global_5, global_4, vec2<f32>(((atan2(((_e586 * _e579.x) + (_e587 * _e579.z)), ((_e587 * _e579.x) - (_e586 * _e579.z))) * 0.15915494f) + 0.5f), (acos(clamp(_e579.y, -1f, 1f)) * 0.31830987f)), 0f);
                                    let _e607 = local_7;
                                    phi_7707_ = (_e604.xyz * _e607);
                                    break;
                                }
                                let _e615 = normalize(cross(_e266, select(vec3<f32>(0f, 0f, 1f), vec3<f32>(1f, 0f, 0f), vec3((abs(_e266.z) > 0.98f)))));
                                let _e619 = dot(_e579, _e266);
                                let _e629 = ((((asin(clamp(_e619, -1f, 1f)) * 0.31830987f) + 0.5f) * 2f) - 1f);
                                let _e635 = (0.5f + ((0.5f * sign(_e629)) * sqrt(abs(_e629))));
                                let _e636 = vec2<f32>(((atan2(dot(_e579, normalize(cross(_e615, _e266))), dot(_e579, _e615)) * 0.15915494f) + 0.5f), _e635);
                                let _e641 = textureSampleLevel(global_5, global_4, vec2<f32>(_e636.x, clamp(_e635, 0.001953125f, 0.9980469f)), 0f);
                                let _e644 = global_1.member_24;
                                let _e645 = (_e644 == 2u);
                                let _e647 = global_1.member_8;
                                let _e649 = global_1.member_9;
                                let _e651 = global_1.member_10;
                                let _e653 = global_1.member_18;
                                let _e655 = global_1.member_23;
                                let _e657 = global_1.member_17;
                                let _e659 = global_1.member_13;
                                let _e663 = vec3(clamp(_e218, 0f, 1f));
                                let _e664 = mix((_e647 * max(_e649, 0f)), _e651, _e663);
                                phi_7705_ = _e641.xyz;
                                if (_e645 && (_e619 < 0f)) {
                                    phi_7705_ = mix(mix(_e664, _e655, vec3(clamp(_e653, 0f, 1f))), _e664, vec3(clamp((-(_e619) * max(_e657, 0.001f)), 0f, 1f)));
                                }
                                let _e677 = phi_7705_;
                                phi_7706_ = _e677;
                                if ((_e218 > 0f) && (_e619 > select(-1f, 0f, _e645))) {
                                    if _e645 {
                                        phi_7703_ = max(_e619, 0f);
                                    } else {
                                        phi_7703_ = abs(_e619);
                                    }
                                    let _e685 = phi_7703_;
                                    let _e697 = fract((((normalize(_e579) * 37f) * 0.3183099f) + vec3<f32>(0.1f, 0.2f, 0.3f)));
                                    let _e702 = (_e697 + vec3(dot(_e697, (_e697.yzx + vec3<f32>(19.19f, 19.19f, 19.19f)))));
                                    phi_7706_ = mix(_e677, (mix(mix(_e677, vec3<f32>(0.00018f, 0.00009f, 0.0006f), vec3(smoothstep(0f, 0.85f, _e685))), _e659, vec3((1f - smoothstep(0f, 0.55f, _e685)))) * (1f + ((fract(((_e702.x * _e702.y) * _e702.z)) - 0.5f) * 0.05f))), _e663);
                                }
                                let _e715 = phi_7706_;
                                phi_7707_ = _e715;
                                break;
                            }
                        }
                        let _e717 = phi_7707_;
                        if (_e225 < 0.5f) {
                            phi_7708_ = mix(_e440, _e578, vec3(smoothstep(0f, 0.5f, _e225)));
                        } else {
                            phi_7708_ = mix(_e578, _e717, vec3(smoothstep(0.5f, 1f, _e225)));
                        }
                        let _e726 = phi_7708_;
                        let _e728 = global_1.member_15;
                        let _e730 = global_1.member_16;
                        let _e739 = mix(_e726, _e578, vec3(clamp((1f - smoothstep(0f, max((mix(_e728, _e730, _e218) * 3f), 0.015f), abs(_e222))), 0f, 1f)));
                        let _e741 = global.member_8;
                        let _e742 = normalize(_e741);
                        let _e744 = global.member_9;
                        let _e745 = (_e744 > 0f);
                        phi_3686_ = _e745;
                        if _e745 {
                            phi_3686_ = (_e742.y > -0.18f);
                        }
                        let _e749 = phi_3686_;
                        phi_7722_ = _e739;
                        if (_e749 && (_e218 < 0.98f)) {
                            let _e753 = global.member_2;
                            let _e754 = normalize(_e753);
                            let _e756 = global.member_4;
                            let _e757 = normalize(_e756);
                            let _e760 = global.member_3;
                            let _e764 = max(((_e744 * max(_e760, 0.1f)) * 16f), 0.055f);
                            let _e766 = global.member_13;
                            if (_e766 > 0.5f) {
                                let _e770 = vec2<f32>(dot(_e742, _e754), dot(_e742, _e757));
                                let _e771 = length(_e770);
                                phi_7709_ = _e770;
                                if (_e771 > 0.82f) {
                                    phi_7709_ = (_e770 * (0.82f / _e771));
                                }
                                let _e776 = phi_7709_;
                                let _e783 = vec2<f32>(((_e204 - (_e776.x / _e213)) * _e213), (_e222 - _e776.y));
                                let _e785 = -(dot(_e783, _e783));
                                let _e786 = (_e764 * _e764);
                                phi_7714_ = exp((_e785 / max(_e786, 0.000001f)));
                                phi_7710_ = exp((_e785 / max((_e786 * 26f), 0.000001f)));
                            } else {
                                let _e800 = vec2<f32>(((_e204 - (dot(_e742, _e754) / _e213)) * _e213), (_e222 - dot(_e742, _e757)));
                                let _e802 = smoothstep(-0.05f, 0.05f, dot(_e742, normalize(_e268)));
                                let _e804 = -(dot(_e800, _e800));
                                let _e805 = (_e764 * _e764);
                                phi_7714_ = (exp((_e804 / max(_e805, 0.000001f))) * _e802);
                                phi_7710_ = (exp((_e804 / max((_e805 * 26f), 0.000001f))) * _e802);
                            }
                            let _e816 = phi_7714_;
                            let _e818 = phi_7710_;
                            let _e820 = smoothstep(-0.05f, 0.05f, _e742.y);
                            let _e821 = smoothstep(-0.006f, 0.006f, _e222);
                            let _e822 = (1f - _e218);
                            let _e826 = global.member_16;
                            let _e830 = global.member_17;
                            phi_7722_ = ((_e739 + (((mix(_e826, vec3<f32>(1f, 0.48f, 0.18f), vec3((1f - smoothstep(0.04f, 0.55f, _e742.y)))) * ((clamp((_e830 * 0.018f), 0f, 1.2f) * _e820) * _e822)) * _e818) * _e821)) + (((_e826 * (((_e830 * 0.4f) * _e820) * _e822)) * _e816) * _e821));
                        }
                        let _e847 = phi_7722_;
                        let _e850 = global.member_30[0u];
                        phi_7721_ = _e847;
                        if ((_e850 > 0.5f) && (_e218 > 0.01f)) {
                            let _e855 = global.member_2;
                            let _e858 = global.member_4;
                            let _e863 = global.member_30[1u];
                            let _e867 = global.member_30[2u];
                            let _e871 = global.member_30[3u];
                            let _e872 = clamp(_e871, 0.2f, 50f);
                            let _e875 = global.member_31[0u];
                            let _e876 = clamp(_e875, 0.2f, 4f);
                            let _e879 = global.member_31[1u];
                            let _e880 = clamp(_e879, 0.1f, 4f);
                            let _e883 = global.member_31[3u];
                            phi_7723_ = _e847;
                            phi_7715_ = 0i;
                            loop {
                                let _e886 = phi_7723_;
                                let _e888 = phi_7715_;
                                local_13 = _e886;
                                if (_e888 < 8i) {
                                    let _e890 = f32(_e888);
                                    let _e896 = global.member_24;
                                    let _e903 = (((_e890 * 31.73f) + (floor(((_e896 * 0.017f) + (_e890 * 7.13f))) * 19.19f)) + 3.11f);
                                    let _e907 = fract(((vec3<f32>(_e903, 37f, 9f) * 0.3183099f) + vec3<f32>(0.1f, 0.2f, 0.3f)));
                                    let _e912 = (_e907 + vec3(dot(_e907, (_e907.yzx + vec3<f32>(19.19f, 19.19f, 19.19f)))));
                                    let _e928 = fract(((vec3<f32>(_e903, 2f, 5f) * 0.3183099f) + vec3<f32>(0.1f, 0.2f, 0.3f)));
                                    let _e933 = (_e928 + vec3(dot(_e928, (_e928.yzx + vec3<f32>(19.19f, 19.19f, 19.19f)))));
                                    let _e941 = fract(((_e896 / max((mix(8.5f, 1f, (clamp(_e867, 0f, 4f) * 0.25f)) * mix(0.58f, 1.72f, fract(((_e912.x * _e912.y) * _e912.z)))), 0.1f)) + fract(((_e933.x * _e933.y) * _e933.z))));
                                    let _e945 = fract(((vec3<f32>(_e903, 43f, 17f) * 0.3183099f) + vec3<f32>(0.1f, 0.2f, 0.3f)));
                                    let _e950 = (_e945 + vec3(dot(_e945, (_e945.yzx + vec3<f32>(19.19f, 19.19f, 19.19f)))));
                                    let _e957 = mix(0.55f, 1.65f, fract(((_e950.x * _e950.y) * _e950.z)));
                                    let _e961 = mix(0.42f, 0.13f, clamp(((_e872 * _e957) * 0.25f), 0f, 1f));
                                    let _e970 = fract(((vec3<f32>(_e903, 11f, 7f) * 0.3183099f) + vec3<f32>(0.1f, 0.2f, 0.3f)));
                                    let _e975 = (_e970 + vec3(dot(_e970, (_e970.yzx + vec3<f32>(19.19f, 19.19f, 19.19f)))));
                                    let _e985 = fract(((vec3<f32>(_e903, 41f, 29f) * 0.3183099f) + vec3<f32>(0.1f, 0.2f, 0.3f)));
                                    let _e990 = (_e985 + vec3(dot(_e985, (_e985.yzx + vec3<f32>(19.19f, 19.19f, 19.19f)))));
                                    let _e999 = ((fract(((_e975.x * _e975.y) * _e975.z)) + (0.23f * fract(((_e990.x * _e990.y) * _e990.z)))) * 6.2831855f);
                                    let _e1003 = fract(((vec3<f32>(_e903, 13f, 3f) * 0.3183099f) + vec3<f32>(0.1f, 0.2f, 0.3f)));
                                    let _e1008 = (_e1003 + vec3(dot(_e1003, (_e1003.yzx + vec3<f32>(19.19f, 19.19f, 19.19f)))));
                                    let _e1016 = mix(0.12f, 0.96f, pow(fract(((_e1008.x * _e1008.y) * _e1008.z)), 0.72f));
                                    let _e1020 = sqrt(max(0f, (1f - (_e1016 * _e1016))));
                                    let _e1026 = normalize(vec3<f32>((cos(_e999) * _e1020), _e1016, (sin(_e999) * _e1020)));
                                    let _e1028 = vec2<f32>((_e204 * _e213), _e222);
                                    let _e1032 = vec2<f32>((dot(_e1026, normalize(_e855)) / _e213), dot(_e1026, normalize(_e858)));
                                    let _e1034 = global.member_13;
                                    phi_7727_ = _e1032;
                                    if (_e1034 > 0.5f) {
                                        let _e1036 = length(_e1032);
                                        phi_7728_ = _e1032;
                                        if (_e1036 > 1.18f) {
                                            phi_7728_ = (_e1032 * (1.18f / _e1036));
                                        }
                                        let _e1041 = phi_7728_;
                                        phi_7727_ = _e1041;
                                    }
                                    let _e1043 = phi_7727_;
                                    let _e1047 = fract(((vec3<f32>(_e903, 71f, 5f) * 0.3183099f) + vec3<f32>(0.1f, 0.2f, 0.3f)));
                                    let _e1052 = (_e1047 + vec3(dot(_e1047, (_e1047.yzx + vec3<f32>(19.19f, 19.19f, 19.19f)))));
                                    let _e1059 = mix(-2.55f, -0.58f, fract(((_e1052.x * _e1052.y) * _e1052.z)));
                                    let _e1064 = normalize(vec2<f32>((cos(_e1059) / _e213), sin(_e1059)));
                                    let _e1073 = (vec2<f32>((_e1043.x * _e213), _e1043.y) + (_e1064 * (((_e941 * 0.58f) * _e872) * _e957)));
                                    let _e1077 = fract(((vec3<f32>(_e903, 53f, 11f) * 0.3183099f) + vec3<f32>(0.1f, 0.2f, 0.3f)));
                                    let _e1082 = (_e1077 + vec3(dot(_e1077, (_e1077.yzx + vec3<f32>(19.19f, 19.19f, 19.19f)))));
                                    let _e1089 = mix(0.55f, 1.75f, fract(((_e1082.x * _e1082.y) * _e1082.z)));
                                    let _e1093 = fract(((vec3<f32>(_e903, 61f, 13f) * 0.3183099f) + vec3<f32>(0.1f, 0.2f, 0.3f)));
                                    let _e1098 = (_e1093 + vec3(dot(_e1093, (_e1093.yzx + vec3<f32>(19.19f, 19.19f, 19.19f)))));
                                    let _e1111 = max((((0.3f * _e876) * _e1089) * smoothstep(0f, (_e961 * 0.55f), _e941)), 0.0001f);
                                    let _e1112 = (_e1028 - _e1073);
                                    let _e1114 = dot(_e1112, -(_e1064));
                                    let _e1115 = clamp(_e1114, 0f, _e1111);
                                    let _e1119 = length((_e1028 - (_e1073 - (_e1064 * _e1115))));
                                    switch bitcast<i32>(0u) {
                                        default: {
                                            let _e1145 = (_e1112 / vec2((0.03f * clamp(_e883, 0.1f, 4f))));
                                            let _e1146 = dpdx(_e1145);
                                            let _e1147 = dpdy(_e1145);
                                            let _e1152 = (1f + (0.5f * (length(_e1146) + length(_e1147))));
                                            if (dot(_e1145, _e1145) > (_e1152 * _e1152)) {
                                                phi_7737_ = 0f;
                                                break;
                                            }
                                            phi_7730_ = 0f;
                                            phi_7729_ = 0i;
                                            loop {
                                                let _e1157 = phi_7730_;
                                                let _e1159 = phi_7729_;
                                                local_8 = _e1157;
                                                if (_e1159 < 4i) {
                                                    local_2 = array<vec2<f32>, 4>(vec2<f32>(0.125f, 0.375f), vec2<f32>(0.375f, -0.125f), vec2<f32>(-0.125f, -0.375f), vec2<f32>(-0.375f, 0.125f));
                                                    let _e1163 = local_2[_e1159][0u];
                                                    local_3 = array<vec2<f32>, 4>(vec2<f32>(0.125f, 0.375f), vec2<f32>(0.375f, -0.125f), vec2<f32>(-0.125f, -0.375f), vec2<f32>(-0.375f, 0.125f));
                                                    let _e1168 = local_3[_e1159][1u];
                                                    let _e1170 = ((_e1145 + (_e1146 * _e1163)) + (_e1147 * _e1168));
                                                    let _e1173 = clamp((length(_e1170) * 3.3333333f), 0f, 1f);
                                                    if (_e1173 <= 0.15f) {
                                                        phi_7731_ = 0f;
                                                    } else {
                                                        phi_7731_ = ((_e1173 - 0.15f) * 1.1764705f);
                                                    }
                                                    let _e1178 = phi_7731_;
                                                    let _e1182 = sqrt(abs(_e1170));
                                                    let _e1185 = (_e1182.x + _e1182.y);
                                                    let _e1187 = clamp((_e1185 * _e1185), 0f, 1f);
                                                    if (_e1187 <= 0.15f) {
                                                        phi_7732_ = 0f;
                                                    } else {
                                                        phi_7732_ = ((_e1187 - 0.15f) * 1.1764705f);
                                                    }
                                                    let _e1192 = phi_7732_;
                                                    local_12 = (_e1157 + max(pow((1f - _e1178), 10f), pow((1f - _e1192), 10f)));
                                                    continue;
                                                } else {
                                                    break;
                                                }
                                                continuing {
                                                    let _e2371 = local_12;
                                                    phi_7730_ = _e2371;
                                                    phi_7729_ = (_e1159 + 1i);
                                                }
                                            }
                                            let _e1199 = local_8;
                                            phi_7737_ = (0.25f * _e1199);
                                            break;
                                        }
                                    }
                                    let _e1202 = phi_7737_;
                                    local_11 = (_e886 + ((((((((vec3<f32>(0.62f, 0.78f, 1f) * ((exp(((-(_e1119) * _e1119) * (105000f / max((_e880 * _e880), 0.01f)))) * exp((-(_e1115) * (20f / max((_e876 * _e1089), 0.2f))))) * (smoothstep(0f, 0.006f, _e1114) * (1f - smoothstep((_e1111 * 0.88f), _e1111, _e1114))))) * 0.85f) + ((vec3<f32>(1f, 0.96f, 0.82f) * _e1202) * 2.4f)) * (smoothstep(0f, 0.035f, _e941) * (1f - smoothstep((_e961 * 0.58f), _e961, _e941)))) * step((_e890 + 0.5f), (clamp(_e863, 0f, 1f) * 8f))) * smoothstep(-0.03f, 0.03f, dot(_e1026, normalize(_e268)))) * _e218) * mix(0.55f, 1.85f, fract(((_e1098.x * _e1098.y) * _e1098.z)))));
                                    continue;
                                } else {
                                    break;
                                }
                                continuing {
                                    let _e2365 = local_11;
                                    phi_7723_ = _e2365;
                                    phi_7715_ = (_e888 + 1i);
                                }
                            }
                            let _e2379 = local_13;
                            phi_7721_ = _e2379;
                        }
                        let _e1216 = phi_7721_;
                        let _e1218 = global.member_10;
                        let _e1219 = normalize(_e1218);
                        let _e1221 = smoothstep(-0.18f, 0.02f, _e1219.y);
                        let _e1223 = global.member_11;
                        phi_7725_ = _e1216;
                        if ((_e1223 > 0f) && (_e1221 > 0f)) {
                            let _e1228 = global.member_2;
                            let _e1231 = global.member_4;
                            let _e1235 = vec2<f32>(dot(_e1219, normalize(_e1228)), dot(_e1219, normalize(_e1231)));
                            let _e1237 = global.member_13;
                            phi_7716_ = _e1235;
                            if (_e1237 > 0.5f) {
                                let _e1239 = length(_e1235);
                                phi_7717_ = _e1235;
                                if (_e1239 > 0.82f) {
                                    phi_7717_ = (_e1235 * (0.82f / _e1239));
                                }
                                let _e1244 = phi_7717_;
                                phi_7716_ = _e1244;
                            }
                            let _e1246 = phi_7716_;
                            let _e1251 = global.member_12;
                            let _e1254 = global.member_5;
                            let _e1257 = max(((_e1251 * 6f) * max(_e1254, 0.1f)), 0.05f);
                            let _e1261 = vec2<f32>(((_e204 - (_e1246.x / _e213)) * _e213), (_e222 - _e1246.y));
                            let _e1266 = (smoothstep(1f, 0.92f, (length(_e1261) / _e1257)) * smoothstep(-0.006f, 0.006f, _e222));
                            phi_7726_ = _e1216;
                            if (_e1266 > 0f) {
                                let _e1271 = (((_e1261 / vec2(_e1257)) * 0.5f) + vec2<f32>(0.5f, 0.5f));
                                let _e1276 = vec2<f32>(_e1271.x, (1f - _e1271.y));
                                let _e1277 = textureSampleLevel(global_8, global_7, _e1276, 0f);
                                let _e1279 = global.member_14;
                                switch bitcast<i32>(0u) {
                                    default: {
                                        let _e1282 = ((_e1276 * 2f) - vec2<f32>(1f, 1f));
                                        let _e1283 = dot(_e1282, _e1282);
                                        if (_e1283 > 1f) {
                                            phi_7718_ = 0f;
                                            break;
                                        }
                                        let _e1290 = ((fract(_e1279) - 0.5f) * 6.2831855f);
                                        phi_7718_ = smoothstep(-0.04f, 0.04f, dot(vec3<f32>(_e1282.x, _e1282.y, sqrt(max(0f, (1f - _e1283)))), normalize(vec3<f32>(-(sin(_e1290)), 0f, cos(_e1290)))));
                                        break;
                                    }
                                }
                                let _e1302 = phi_7718_;
                                let _e1309 = global.member_15;
                                phi_7726_ = (_e1216 + ((((((_e1277.xyz * vec3<f32>(0.85f, 0.9f, 1.05f)) * ((_e1223 * exp2(_e1309)) * mix(0.2f, 1f, _e218))) * _e1266) * _e1277.w) * mix(0.08f, 1f, _e1302)) * _e1221));
                            }
                            let _e1320 = phi_7726_;
                            phi_7725_ = _e1320;
                        }
                        let _e1322 = phi_7725_;
                        let _e1324 = global.member_1;
                        let _e1326 = (_e1322 * exp2(_e1324));
                        global_6 = vec4<f32>(_e1326.x, _e1326.y, _e1326.z, 1f);
                        break;
                    }
                }
                break;
            }
            let _e1332 = global.member_2;
            let _e1333 = normalize(_e1332);
            let _e1335 = global.member_4;
            let _e1336 = normalize(_e1335);
            let _e1338 = global.member_6;
            let _e1344 = normalize(vec3<f32>(((_e204 * _e213) * _e210), (_e208 * _e210), -1f));
            let _e1354 = normalize((((_e1333 * _e1344.x) + (_e1336 * _e1344.y)) + (normalize(_e1338) * -(_e1344.z))));
            let _e1357 = global.member_32[3u];
            if (_e1357 >= 0.5f) {
                let _e1360 = max(_e1354.y, 0f);
                let _e1362 = max(-(_e1354.y), 0f);
                let _e1366 = global.member_32;
                let _e1370 = global.member_33;
                let _e1375 = global.member_34;
                let _e1380 = global.member_1;
                let _e1382 = ((((_e1366.xyz * _e1360) + (_e1370.xyz * ((1f - _e1360) - _e1362))) + (_e1375.xyz * _e1362)) * exp2(_e1380));
                global_6 = vec4<f32>(_e1382.x, _e1382.y, _e1382.z, 1f);
                break;
            }
            let _e1389 = global.member_29[3u];
            if (_e1389 >= 0f) {
                let _e1393 = global.member_29[2u];
                let _e1394 = sin(_e1393);
                let _e1395 = cos(_e1393);
                let _e1412 = textureSampleLevel(global_5, global_4, vec2<f32>(((atan2(((_e1394 * _e1354.x) + (_e1395 * _e1354.z)), ((_e1395 * _e1354.x) - (_e1394 * _e1354.z))) * 0.15915494f) + 0.5f), (acos(clamp(_e1354.y, -1f, 1f)) * 0.31830987f)), 0f);
                let _e1416 = global.member_1;
                let _e1418 = ((_e1412.xyz * _e1389) * exp2(_e1416));
                global_6 = vec4<f32>(_e1418.x, _e1418.y, _e1418.z, 1f);
                break;
            }
            let _e1424 = global_1.member;
            let _e1426 = vec3<f32>(0f, -(_e1424), 0f);
            let _e1428 = global.member;
            let _e1429 = (_e1428 - _e1426);
            let _e1430 = length(_e1429);
            let _e1433 = (_e1429 / vec3(max(_e1430, 0.00001f)));
            let _e1434 = dot(_e1354, _e1433);
            let _e1436 = global.member_8;
            let _e1437 = normalize(_e1436);
            let _e1439 = global.member_23;
            let _e1440 = clamp(_e1439, 0f, 1f);
            let _e1447 = normalize(cross(_e1433, select(vec3<f32>(0f, 0f, 1f), vec3<f32>(1f, 0f, 0f), vec3((abs(_e1433.z) > 0.98f)))));
            let _e1460 = ((((asin(clamp(_e1434, -1f, 1f)) * 0.31830987f) + 0.5f) * 2f) - 1f);
            let _e1466 = (0.5f + ((0.5f * sign(_e1460)) * sqrt(abs(_e1460))));
            let _e1467 = vec2<f32>(((atan2(dot(_e1354, normalize(cross(_e1447, _e1433))), dot(_e1354, _e1447)) * 0.15915494f) + 0.5f), _e1466);
            let _e1472 = textureSampleLevel(global_5, global_4, vec2<f32>(_e1467.x, clamp(_e1466, 0.001953125f, 0.9980469f)), 0f);
            let _e1474 = dot(_e1354, _e1437);
            let _e1476 = global.member_9;
            phi_7577_ = 0f;
            if ((_e1476 > 0f) && (_e1474 > 0f)) {
                let _e1482 = max(_e1476, 0.000001f);
                let _e1483 = (_e1482 * 0.25f);
                phi_7577_ = (clamp((1f - smoothstep((_e1482 - _e1483), (_e1482 + _e1483), acos(clamp(_e1474, -1f, 1f)))), 0f, 1f) * (1f - _e1440));
            }
            let _e1492 = phi_7577_;
            let _e1494 = (_e1429 / vec3(_e1430));
            let _e1496 = clamp(dot(_e1354, _e1494), -1f, 1f);
            let _e1498 = global_1.member_1;
            switch bitcast<i32>(0u) {
                default: {
                    let _e1500 = clamp(_e1430, _e1424, _e1498);
                    let _e1501 = (_e1500 * _e1496);
                    let _e1502 = (_e1496 < 0f);
                    phi_6016_ = _e1502;
                    if _e1502 {
                        phi_6016_ = (((_e1501 * _e1501) - ((_e1500 - _e1424) * (_e1500 + _e1424))) >= 0f);
                    }
                    let _e1510 = phi_6016_;
                    if _e1510 {
                        phi_7574_ = vec3<f32>(0f, 0f, 0f);
                        break;
                    }
                    let _e1511 = textureDimensions(global_3, 0i);
                    let _e1513 = vec2<f32>(vec2<i32>(_e1511));
                    let _e1517 = sqrt(((_e1498 - _e1424) * (_e1498 + _e1424)));
                    let _e1522 = sqrt(max(((_e1500 - _e1424) * (_e1500 + _e1424)), 0f));
                    let _e1523 = (_e1498 - _e1500);
                    let _e1525 = (_e1523 * (_e1498 + _e1500));
                    let _e1529 = sqrt(max((_e1525 + (_e1501 * _e1501)), 0f));
                    if (_e1501 > 0f) {
                        phi_7573_ = (_e1525 / (_e1529 + _e1501));
                    } else {
                        phi_7573_ = (_e1529 - _e1501);
                    }
                    let _e1535 = phi_7573_;
                    let _e1557 = textureSampleLevel(global_3, global_2, vec2<f32>(((0.5f / _e1513.x) + (clamp(((_e1535 - _e1523) / max(((_e1522 + _e1517) - _e1523), 0.001f)), 0f, 1f) * (1f - (1f / _e1513.x)))), ((0.5f / _e1513.y) + (clamp((_e1522 / _e1517), 0f, 1f) * (1f - (1f / _e1513.y))))), 0f);
                    phi_7574_ = _e1557.xyz;
                    break;
                }
            }
            let _e1560 = phi_7574_;
            let _e1562 = global.member_1;
            let _e1563 = exp2(_e1562);
            let _e1565 = global.member_17;
            let _e1566 = max(_e1476, 0.000001f);
            let _e1578 = global.member_16;
            let _e1583 = global.member_11;
            phi_7652_ = vec3<f32>(0f, 0f, 0f);
            if (_e1583 > 0f) {
                let _e1586 = global.member_10;
                let _e1587 = normalize(_e1586);
                let _e1588 = dot(_e1354, _e1587);
                let _e1590 = smoothstep(0f, 0.02f, dot(_e1587, _e1433));
                let _e1592 = global.member_12;
                let _e1595 = max(0.000001f, select(0.026f, _e1592, (_e1592 > 0f)));
                phi_7653_ = vec3<f32>(0f, 0f, 0f);
                if (((_e1588 > cos((_e1595 * 1.02f))) && (_e1434 > 0f)) && (_e1590 > 0f)) {
                    let _e1615 = normalize(cross(select(vec3<f32>(1f, 0f, 0f), vec3<f32>(0f, 1f, 0f), vec3((abs(_e1587.y) < 0.99f))), _e1587));
                    let _e1618 = (_e1354 - (_e1587 * _e1588));
                    let _e1627 = vec2<f32>((((dot(_e1618, _e1615) / _e1595) * 0.5f) + 0.5f), (0.5f - ((dot(_e1618, cross(_e1587, _e1615)) / _e1595) * 0.5f)));
                    let _e1628 = textureSampleLevel(global_8, global_7, _e1627, 0f);
                    let _e1630 = global.member_14;
                    switch bitcast<i32>(0u) {
                        default: {
                            let _e1633 = ((_e1627 * 2f) - vec2<f32>(1f, 1f));
                            let _e1634 = dot(_e1633, _e1633);
                            if (_e1634 > 1f) {
                                phi_7580_ = 0f;
                                break;
                            }
                            let _e1641 = ((fract(_e1630) - 0.5f) * 6.2831855f);
                            phi_7580_ = smoothstep(-0.04f, 0.04f, dot(vec3<f32>(_e1633.x, _e1633.y, sqrt(max(0f, (1f - _e1634)))), normalize(vec3<f32>(-(sin(_e1641)), 0f, cos(_e1641)))));
                            break;
                        }
                    }
                    let _e1653 = phi_7580_;
                    let _e1665 = global.member_15;
                    phi_7653_ = (((_e1628.xyz * vec3<f32>(0.85f, 0.9f, 1.05f)) * ((_e1583 * exp2(_e1665)) * mix(0.15f, 1f, _e1440))) * (smoothstep(1f, 0.92f, (sqrt(max(0f, (2f * (1f - _e1588)))) / _e1595)) * (((smoothstep(0f, 0.02f, _e1434) * _e1590) * _e1628.w) * mix(0.08f, 1f, _e1653))));
                }
                let _e1672 = phi_7653_;
                phi_7652_ = _e1672;
            }
            let _e1674 = phi_7652_;
            let _e1677 = global.member_30[0u];
            phi_7636_ = vec3<f32>(0f, 0f, 0f);
            if ((_e1677 > 0.5f) && (_e1440 > 0.01f)) {
                let _e1683 = global.member_30[1u];
                let _e1687 = global.member_30[2u];
                let _e1691 = global.member_30[3u];
                let _e1692 = clamp(_e1691, 0.2f, 50f);
                let _e1695 = global.member_31[0u];
                let _e1696 = clamp(_e1695, 0.2f, 4f);
                let _e1699 = global.member_31[1u];
                let _e1700 = clamp(_e1699, 0.1f, 4f);
                let _e1703 = global.member_31[2u];
                phi_7637_ = vec3<f32>(0f, 0f, 0f);
                phi_7582_ = 0i;
                loop {
                    let _e1706 = phi_7637_;
                    let _e1708 = phi_7582_;
                    local_16 = _e1706;
                    if (_e1708 < 8i) {
                        let _e1710 = f32(_e1708);
                        let _e1716 = global.member_24;
                        let _e1723 = (((_e1710 * 31.73f) + (floor(((_e1716 * 0.017f) + (_e1710 * 7.13f))) * 19.19f)) + 3.11f);
                        let _e1727 = fract(((vec3<f32>(_e1723, 37f, 9f) * 0.3183099f) + vec3<f32>(0.1f, 0.2f, 0.3f)));
                        let _e1732 = (_e1727 + vec3(dot(_e1727, (_e1727.yzx + vec3<f32>(19.19f, 19.19f, 19.19f)))));
                        let _e1748 = fract(((vec3<f32>(_e1723, 2f, 5f) * 0.3183099f) + vec3<f32>(0.1f, 0.2f, 0.3f)));
                        let _e1753 = (_e1748 + vec3(dot(_e1748, (_e1748.yzx + vec3<f32>(19.19f, 19.19f, 19.19f)))));
                        let _e1761 = fract(((_e1716 / max((mix(8.5f, 1f, (clamp(_e1687, 0f, 4f) * 0.25f)) * mix(0.58f, 1.72f, fract(((_e1732.x * _e1732.y) * _e1732.z)))), 0.1f)) + fract(((_e1753.x * _e1753.y) * _e1753.z))));
                        let _e1765 = fract(((vec3<f32>(_e1723, 43f, 17f) * 0.3183099f) + vec3<f32>(0.1f, 0.2f, 0.3f)));
                        let _e1770 = (_e1765 + vec3(dot(_e1765, (_e1765.yzx + vec3<f32>(19.19f, 19.19f, 19.19f)))));
                        let _e1777 = mix(0.55f, 1.65f, fract(((_e1770.x * _e1770.y) * _e1770.z)));
                        let _e1781 = mix(0.42f, 0.13f, clamp(((_e1692 * _e1777) * 0.25f), 0f, 1f));
                        let _e1790 = fract(((vec3<f32>(_e1723, 11f, 7f) * 0.3183099f) + vec3<f32>(0.1f, 0.2f, 0.3f)));
                        let _e1795 = (_e1790 + vec3(dot(_e1790, (_e1790.yzx + vec3<f32>(19.19f, 19.19f, 19.19f)))));
                        let _e1805 = fract(((vec3<f32>(_e1723, 41f, 29f) * 0.3183099f) + vec3<f32>(0.1f, 0.2f, 0.3f)));
                        let _e1810 = (_e1805 + vec3(dot(_e1805, (_e1805.yzx + vec3<f32>(19.19f, 19.19f, 19.19f)))));
                        let _e1819 = ((fract(((_e1795.x * _e1795.y) * _e1795.z)) + (0.23f * fract(((_e1810.x * _e1810.y) * _e1810.z)))) * 6.2831855f);
                        let _e1823 = fract(((vec3<f32>(_e1723, 13f, 3f) * 0.3183099f) + vec3<f32>(0.1f, 0.2f, 0.3f)));
                        let _e1828 = (_e1823 + vec3(dot(_e1823, (_e1823.yzx + vec3<f32>(19.19f, 19.19f, 19.19f)))));
                        let _e1836 = mix(0.12f, 0.96f, pow(fract(((_e1828.x * _e1828.y) * _e1828.z)), 0.72f));
                        let _e1840 = sqrt(max(0f, (1f - (_e1836 * _e1836))));
                        let _e1846 = normalize(vec3<f32>((cos(_e1819) * _e1840), _e1836, (sin(_e1819) * _e1840)));
                        let _e1850 = fract(((vec3<f32>(_e1723, 17f, 2f) * 0.3183099f) + vec3<f32>(0.1f, 0.2f, 0.3f)));
                        let _e1855 = (_e1850 + vec3(dot(_e1850, (_e1850.yzx + vec3<f32>(19.19f, 19.19f, 19.19f)))));
                        let _e1866 = fract(((vec3<f32>(_e1723, 23f, 31f) * 0.3183099f) + vec3<f32>(0.1f, 0.2f, 0.3f)));
                        let _e1871 = (_e1866 + vec3(dot(_e1866, (_e1866.yzx + vec3<f32>(19.19f, 19.19f, 19.19f)))));
                        let _e1882 = fract(((vec3<f32>(_e1723, 19f, 5f) * 0.3183099f) + vec3<f32>(0.1f, 0.2f, 0.3f)));
                        let _e1887 = (_e1882 + vec3(dot(_e1882, (_e1882.yzx + vec3<f32>(19.19f, 19.19f, 19.19f)))));
                        let _e1896 = normalize(vec3<f32>(mix(-0.9f, 0.9f, fract(((_e1855.x * _e1855.y) * _e1855.z))), mix(-1.35f, -0.45f, fract(((_e1871.x * _e1871.y) * _e1871.z))), mix(-0.9f, 0.9f, fract(((_e1887.x * _e1887.y) * _e1887.z)))));
                        let _e1900 = normalize((_e1896 - (_e1846 * dot(_e1896, _e1846))));
                        let _e1906 = normalize((_e1846 + (_e1900 * (((_e1761 * 0.8f) * _e1692) * _e1777))));
                        let _e1909 = (_e1354 - _e1906);
                        let _e1913 = fract(((vec3<f32>(_e1723, 53f, 11f) * 0.3183099f) + vec3<f32>(0.1f, 0.2f, 0.3f)));
                        let _e1918 = (_e1913 + vec3(dot(_e1913, (_e1913.yzx + vec3<f32>(19.19f, 19.19f, 19.19f)))));
                        let _e1925 = mix(0.55f, 1.75f, fract(((_e1918.x * _e1918.y) * _e1918.z)));
                        let _e1929 = fract(((vec3<f32>(_e1723, 61f, 13f) * 0.3183099f) + vec3<f32>(0.1f, 0.2f, 0.3f)));
                        let _e1934 = (_e1929 + vec3(dot(_e1929, (_e1929.yzx + vec3<f32>(19.19f, 19.19f, 19.19f)))));
                        let _e1947 = max((((0.1f * _e1696) * _e1925) * smoothstep(0f, (_e1781 * 0.55f), _e1761)), 0.0001f);
                        let _e1949 = dot(_e1909, -(_e1900));
                        let _e1950 = clamp(_e1949, 0f, _e1947);
                        let _e1954 = abs(dot((_e1909 + (_e1900 * _e1950)), normalize(cross(_e1906, _e1900))));
                        let _e1956 = smoothstep(0.7648422f, 0.9393727f, dot(_e1354, _e1906));
                        switch bitcast<i32>(0u) {
                            default: {
                                let _e1984 = (vec2<f32>(dot(_e1909, _e1333), dot(_e1909, _e1336)) / vec2((0.15f * clamp(_e1703, 0.1f, 4f))));
                                let _e1985 = dpdx(_e1984);
                                let _e1986 = dpdy(_e1984);
                                let _e1991 = (1f + (0.5f * (length(_e1985) + length(_e1986))));
                                if (dot(_e1984, _e1984) > (_e1991 * _e1991)) {
                                    phi_7673_ = 0f;
                                    break;
                                }
                                phi_7666_ = 0f;
                                phi_7665_ = 0i;
                                loop {
                                    let _e1996 = phi_7666_;
                                    let _e1998 = phi_7665_;
                                    local_9 = _e1996;
                                    if (_e1998 < 4i) {
                                        local = array<vec2<f32>, 4>(vec2<f32>(0.125f, 0.375f), vec2<f32>(0.375f, -0.125f), vec2<f32>(-0.125f, -0.375f), vec2<f32>(-0.375f, 0.125f));
                                        let _e2002 = local[_e1998][0u];
                                        local_1 = array<vec2<f32>, 4>(vec2<f32>(0.125f, 0.375f), vec2<f32>(0.375f, -0.125f), vec2<f32>(-0.125f, -0.375f), vec2<f32>(-0.375f, 0.125f));
                                        let _e2007 = local_1[_e1998][1u];
                                        let _e2009 = ((_e1984 + (_e1985 * _e2002)) + (_e1986 * _e2007));
                                        let _e2012 = clamp((length(_e2009) * 3.3333333f), 0f, 1f);
                                        if (_e2012 <= 0.15f) {
                                            phi_7667_ = 0f;
                                        } else {
                                            phi_7667_ = ((_e2012 - 0.15f) * 1.1764705f);
                                        }
                                        let _e2017 = phi_7667_;
                                        let _e2021 = sqrt(abs(_e2009));
                                        let _e2024 = (_e2021.x + _e2021.y);
                                        let _e2026 = clamp((_e2024 * _e2024), 0f, 1f);
                                        if (_e2026 <= 0.15f) {
                                            phi_7668_ = 0f;
                                        } else {
                                            phi_7668_ = ((_e2026 - 0.15f) * 1.1764705f);
                                        }
                                        let _e2031 = phi_7668_;
                                        local_15 = (_e1996 + max(pow((1f - _e2017), 10f), pow((1f - _e2031), 10f)));
                                        continue;
                                    } else {
                                        break;
                                    }
                                    continuing {
                                        let _e2398 = local_15;
                                        phi_7666_ = _e2398;
                                        phi_7665_ = (_e1998 + 1i);
                                    }
                                }
                                let _e2038 = local_9;
                                phi_7673_ = (0.25f * _e2038);
                                break;
                            }
                        }
                        let _e2041 = phi_7673_;
                        local_14 = (_e1706 + (((((((vec3<f32>(0.62f, 0.78f, 1f) * (((exp(((-(_e1954) * _e1954) * (340000f / max((_e1700 * _e1700), 0.01f)))) * exp((-(_e1950) * (44f / max((_e1696 * _e1925), 0.2f))))) * _e1956) * (smoothstep(0f, 0.002f, _e1949) * (1f - smoothstep((_e1947 * 0.88f), _e1947, _e1949))))) * 0.85f) + ((vec3<f32>(1f, 0.96f, 0.82f) * (_e2041 * _e1956)) * 2.4f)) * (smoothstep(0f, 0.035f, _e1761) * (1f - smoothstep((_e1781 * 0.58f), _e1781, _e1761)))) * step((_e1710 + 0.5f), (clamp(_e1683, 0f, 1f) * 8f))) * _e1440) * mix(0.55f, 1.85f, fract(((_e1934.x * _e1934.y) * _e1934.z)))));
                        continue;
                    } else {
                        break;
                    }
                    continuing {
                        let _e2394 = local_14;
                        phi_7637_ = _e2394;
                        phi_7582_ = (_e1708 + 1i);
                    }
                }
                let _e2406 = local_16;
                phi_7636_ = _e2406;
            }
            let _e2055 = phi_7636_;
            let _e2057 = global_1.member_24;
            let _e2058 = (_e2057 == 2u);
            let _e2060 = global_1.member_8;
            let _e2062 = global_1.member_9;
            let _e2064 = global_1.member_10;
            let _e2066 = global_1.member_18;
            let _e2068 = global_1.member_23;
            let _e2070 = global_1.member_17;
            let _e2072 = global_1.member_13;
            let _e2073 = max(_e2062, 0f);
            let _e2076 = vec3(clamp(_e1440, 0f, 1f));
            let _e2077 = mix((_e2060 * _e2073), _e2064, _e2076);
            phi_7595_ = _e1472.xyz;
            if (_e2058 && (_e1434 < 0f)) {
                phi_7595_ = mix(mix(_e2077, _e2068, vec3(clamp(_e2066, 0f, 1f))), _e2077, vec3(clamp((-(_e1434) * max(_e2070, 0.001f)), 0f, 1f)));
            }
            let _e2090 = phi_7595_;
            phi_7596_ = _e2090;
            if ((_e1440 > 0f) && (_e1434 > select(-1f, 0f, _e2058))) {
                if _e2058 {
                    phi_7593_ = max(_e1434, 0f);
                } else {
                    phi_7593_ = abs(_e1434);
                }
                let _e2098 = phi_7593_;
                let _e2110 = fract((((normalize(_e1354) * 37f) * 0.3183099f) + vec3<f32>(0.1f, 0.2f, 0.3f)));
                let _e2115 = (_e2110 + vec3(dot(_e2110, (_e2110.yzx + vec3<f32>(19.19f, 19.19f, 19.19f)))));
                phi_7596_ = mix(_e2090, (mix(mix(_e2090, vec3<f32>(0.00018f, 0.00009f, 0.0006f), vec3(smoothstep(0f, 0.85f, _e2098))), _e2072, vec3((1f - smoothstep(0f, 0.55f, _e2098)))) * (1f + ((fract(((_e2115.x * _e2115.y) * _e2115.z)) - 0.5f) * 0.05f))), _e2076);
            }
            let _e2128 = phi_7596_;
            phi_7630_ = _e2128;
            if ((_e2057 == 1u) && (_e1434 < 0.005f)) {
                phi_7598_ = vec3<f32>(0f, 0f, 0f);
                phi_7597_ = 0i;
                loop {
                    let _e2133 = phi_7598_;
                    let _e2135 = phi_7597_;
                    local_10 = _e2133;
                    if (_e2135 < 16i) {
                        continue;
                    } else {
                        break;
                    }
                    continuing {
                        let _e2137 = f32(_e2135);
                        let _e2139 = ((_e2137 + 0.5f) * 0.0625f);
                        let _e2140 = sqrt(_e2139);
                        let _e2144 = (_e2137 * 2.3999631f);
                        let _e2157 = ((((asin(clamp(sqrt(max((1f - _e2139), 0f)), -1f, 1f)) * 0.31830987f) + 0.5f) * 2f) - 1f);
                        let _e2163 = (0.5f + ((0.5f * sign(_e2157)) * sqrt(abs(_e2157))));
                        let _e2164 = vec2<f32>(((atan2((_e2140 * sin(_e2144)), (_e2140 * cos(_e2144))) * 0.15915494f) + 0.5f), _e2163);
                        let _e2169 = textureSampleLevel(global_5, global_4, vec2<f32>(_e2164.x, clamp(_e2163, 0.001953125f, 0.9980469f)), 0f);
                        phi_7598_ = (_e2133 + _e2169.xyz);
                        phi_7597_ = (_e2135 + 1i);
                    }
                }
                let _e2174 = local_10;
                let _e2177 = global_1.member_4;
                let _e2179 = global_1.member_2;
                let _e2181 = global_1.member_25;
                switch bitcast<i32>(0u) {
                    default: {
                        let _e2184 = (_e1430 - _e1424);
                        if (_e2184 <= (_e1424 * 0.0000008f)) {
                            phi_7627_ = 0f;
                            phi_7619_ = select(vec3<f32>(0f, 1f, 0f), _e1494, vec3((_e1430 > 0f)));
                        } else {
                            switch bitcast<i32>(0u) {
                                default: {
                                    let _e2190 = (_e1426 - _e1428);
                                    let _e2191 = dot(_e2190, _e1354);
                                    let _e2193 = (_e2190 - (_e1354 * _e2191));
                                    let _e2194 = dot(_e2193, _e2193);
                                    let _e2195 = (_e1424 * _e1424);
                                    if (_e2194 > _e2195) {
                                        phi_7601_ = f32();
                                        phi_7600_ = false;
                                        break;
                                    }
                                    phi_7601_ = (_e2191 - sqrt(max((_e2195 - _e2194), 0f)));
                                    phi_7600_ = true;
                                    break;
                                }
                            }
                            let _e2202 = phi_7601_;
                            let _e2204 = phi_7600_;
                            if (!(_e2204) || (_e2202 <= 0f)) {
                                phi_7629_ = _e2128;
                                break;
                            }
                            phi_7627_ = _e2202;
                            phi_7619_ = normalize(((_e1428 + (_e1354 * _e2202)) - _e1426));
                        }
                        let _e2213 = phi_7627_;
                        let _e2215 = phi_7619_;
                        let _e2216 = dot(_e2215, _e1437);
                        switch bitcast<i32>(0u) {
                            default: {
                                let _e2218 = clamp(_e1424, _e1424, _e1498);
                                let _e2219 = (_e2218 * _e2216);
                                let _e2220 = (_e2216 < 0f);
                                phi_7207_ = _e2220;
                                if _e2220 {
                                    phi_7207_ = (((_e2219 * _e2219) - ((_e2218 - _e1424) * (_e2218 + _e1424))) >= 0f);
                                }
                                let _e2228 = phi_7207_;
                                if _e2228 {
                                    phi_7621_ = vec3<f32>(0f, 0f, 0f);
                                    break;
                                }
                                let _e2229 = textureDimensions(global_3, 0i);
                                let _e2231 = vec2<f32>(vec2<i32>(_e2229));
                                let _e2235 = sqrt(((_e1498 - _e1424) * (_e1498 + _e1424)));
                                let _e2240 = sqrt(max(((_e2218 - _e1424) * (_e2218 + _e1424)), 0f));
                                let _e2241 = (_e1498 - _e2218);
                                let _e2243 = (_e2241 * (_e1498 + _e2218));
                                let _e2247 = sqrt(max((_e2243 + (_e2219 * _e2219)), 0f));
                                if (_e2219 > 0f) {
                                    phi_7620_ = (_e2243 / (_e2247 + _e2219));
                                } else {
                                    phi_7620_ = (_e2247 - _e2219);
                                }
                                let _e2253 = phi_7620_;
                                let _e2275 = textureSampleLevel(global_3, global_2, vec2<f32>(((0.5f / _e2231.x) + (clamp(((_e2253 - _e2241) / max(((_e2240 + _e2235) - _e2241), 0.001f)), 0f, 1f) * (1f - (1f / _e2231.x)))), ((0.5f / _e2231.y) + (clamp((_e2240 / _e2235), 0f, 1f) * (1f - (1f / _e2231.y))))), 0f);
                                phi_7621_ = _e2275.xyz;
                                break;
                            }
                        }
                        let _e2278 = phi_7621_;
                        phi_7629_ = mix(_e2128, mix(_e2128, mix((((((((_e2060 * 0.31830987f) * max(_e2216, 0f)) * _e2278) * _e1578) * _e1565) + (_e2060 * (_e2174 * vec3<f32>(0.0625f, 0.0625f, 0.0625f)))) * _e2073), _e2064, _e2076), vec3(exp((-(_e2213) * ((dot(_e2177, vec3<f32>(0.299f, 0.587f, 0.114f)) * exp((-(_e2184) / _e2179))) * max(_e2181, 0f)))))), vec3(smoothstep(-0.004f, 0f, -(_e1434))));
                        break;
                    }
                }
                let _e2306 = phi_7629_;
                phi_7630_ = _e2306;
            }
            let _e2308 = phi_7630_;
            phi_7631_ = _e2308;
            if _e2058 {
                let _e2310 = global_1.member_15;
                let _e2312 = global_1.member_16;
                let _e2319 = global_1.member_11;
                let _e2321 = global_1.member_12;
                phi_7631_ = mix(_e2308, mix(_e2319, _e2321, vec3(_e1440)), vec3(clamp((1f - smoothstep(0f, max(mix(_e2310, _e2312, _e1440), 0.00001f), abs(_e1434))), 0f, 1f)));
            }
            let _e2328 = phi_7631_;
            let _e2332 = ((((_e2328 + _e2055) + (((_e1578 * min((_e1565 / (3.1415927f * ((_e1566 * _e1566) + ((_e1566 * 0.05f) * (_e1566 * 0.25f))))), (32752f / max(_e1563, 0.000001f)))) * _e1492) * _e1560)) + _e1674) * _e1563);
            global_6 = vec4<f32>(_e2332.x, _e2332.y, _e2332.z, 1f);
            break;
        }
    }
    return;
}

@fragment
fn main(@location(0) param: vec2<f32>) -> @location(0) vec4<f32> {
    global_9 = param;
    function_();
    let _e3 = global_6;
    return _e3;
}
