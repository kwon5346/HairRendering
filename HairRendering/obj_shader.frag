#version 330 core

in vec3 FragPos;
in vec3 Normal;

out vec4 FragColor;

uniform vec3 lightPos; 
uniform vec3 viewPos;  
uniform float gamma;  

void main()
{
    // Ambient Lighting 
    float ambientStrength = 0.2;
    vec3 ambient = ambientStrength * vec3(1.0); 

    // Diffuse Lighting 
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * vec3(1.0); 

    // Specular Lighting
    float specularStrength = 0.25;
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
    vec3 specular = specularStrength * spec * vec3(1.0);

    // 피부톤 
    vec3 objectColor = vec3(1.0, 0.7, 0.55);

    vec3 result = (ambient + diffuse + specular) * objectColor;

    result = pow(result, vec3(1.0 / gamma)); // 감마 값

    FragColor = vec4(result, 1.0);
}
