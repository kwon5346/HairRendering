#version 330 core

in vec2 tex_coord;

uniform sampler2D mesh_map;        // �Ӹ� ��� (obj)
uniform sampler2D strands_map;     // �Ӹ�ī�� ���� ���
uniform sampler2D slab_map;       // �Ӹ�ī�� ���� ���� (R32UI �ؽ�ó)

out vec4 frag_color;

void main()
{
    vec4 mesh_color = texture(mesh_map, tex_coord);
    vec4 hair_color = texture(strands_map, tex_coord);
    vec4 slab = texture(slab_map, tex_coord);

    vec3 background_color = mesh_color.rgb;

    float n = slab.r + slab.g + slab.b + slab.a;
    float alpha = hair_color.a;
    // ������ �Ӹ�ī�� �е��� ������� alpha ����
    float back_alpha = pow(1.0 - alpha, n);
    background_color *= back_alpha;

    vec3 result = mix(hair_color.rgb, mesh_color.rgb, back_alpha);

    // ���� ������ ���� ��� ���� �� ����
    //result = pow(result, vec3(1.0 / gamma));
    frag_color = vec4(result, 1.0f);
}