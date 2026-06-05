#version 150 core

uniform int uShadingMode;
uniform vec3 uColor;
uniform vec3 uLightPosEye;
uniform vec3 uLightColor;
uniform float uAmbient;
uniform float uKd;
uniform float uKs;
uniform float uShininess;

uniform sampler2D uTexture;
uniform int uTextureEnabled;

in vec3 vAmbientDiffuse;
in vec3 vSpecular;
in vec3 vNormalEye;
in vec3 vPosEye;
in vec2 fTexCoord;

out vec4 fColor;

void main()
{
    vec3 texColor = vec3(1.0);
    if (uTextureEnabled != 0) {
        texColor = texture(uTexture, fTexCoord).rgb;
    }

    vec3 ambientDiffuse;
    vec3 specular;
    if (uShadingMode == 3) {
        vec3 N = normalize(vNormalEye);
        vec3 L = normalize(uLightPosEye - vPosEye);
        float ndotl = max(dot(N, L), 0.0);
        vec3 ambient = uAmbient * uLightColor * uColor;
        vec3 diffuse = uKd * ndotl * uLightColor * uColor;
        ambientDiffuse = ambient + diffuse;
        vec3 V = normalize(-vPosEye);
        vec3 R = reflect(-L, N);
        float spec = pow(max(dot(R, V), 0.0), uShininess);
        specular = uKs * spec * uLightColor;
    } else {
        ambientDiffuse = vAmbientDiffuse;
        specular = vSpecular;
    }

    vec3 rgb = texColor * ambientDiffuse + specular;
    fColor = vec4(clamp(rgb, 0.0, 1.0), 1.0);
}
