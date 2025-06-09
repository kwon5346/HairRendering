#version 330 core

in vec2 tex_coord;

uniform sampler2D mesh_map;        // 머리 배경 (obj)
uniform sampler2D strands_map;     // 머리카락 색상 결과
uniform sampler2D slab_map;       // 머리카락 누적 정보 (R32UI 텍스처)

out vec4 frag_color;

void main()
{
    vec4 mesh_color = texture(mesh_map, tex_coord);
    vec4 hair_color = texture(strands_map, tex_coord);
    vec4 slab = texture(slab_map, tex_coord);

    vec3 background_color = mesh_color.rgb;

    float n = slab.r + slab.g + slab.b + slab.a;
    float alpha = hair_color.a;
    // 누적된 머리카락 밀도를 기반으로 alpha 조정
    float back_alpha = pow(1.0 - alpha, n);
    background_color *= back_alpha;

    vec3 result = mix(hair_color.rgb, mesh_color.rgb, back_alpha);

    // 감마 보정은 최종 출력 직전 한 번만
    //result = pow(result, vec3(1.0 / gamma));
    frag_color = vec4(result, 1.0f);
}