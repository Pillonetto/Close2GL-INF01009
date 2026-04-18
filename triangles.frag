#version 150 core

uniform int uShadingMode;
uniform vec3 uLightPosEye;
uniform vec3 uLightColor;
uniform float uAmbient;
uniform float uKd;
uniform float uKs;
uniform float uShininess;

in vec3 vColor;
in vec3 vNormalEye;
in vec3 vPosEye;

out vec4 fColor;

void main()
{
    if (uShadingMode == 3) {
        vec3 N = normalize(vNormalEye);
        vec3 L = normalize(uLightPosEye - vPosEye);
        float ndotl = max(dot(N, L), 0.0);
        vec3 ambient = uAmbient * uLightColor * vColor;
        vec3 diffuse = uKd * ndotl * uLightColor * vColor;
        vec3 V = normalize(-vPosEye);
        vec3 R = reflect(-L, N);
        float spec = pow(max(dot(R, V), 0.0), uShininess);
        vec3 specular = uKs * spec * uLightColor;
        vec3 rgb = ambient + diffuse + specular;
        fColor = vec4(clamp(rgb, 0.0, 1.0), 1.0);
    } else {
        fColor = vec4(vColor, 1.0);
    }
}
