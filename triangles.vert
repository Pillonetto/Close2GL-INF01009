#version 150 core

// 0 = flat, 1 = Gouraud A+D, 2 = Gouraud A+D+S, 3 = Phong (A+D+S in fragment)
uniform int uShadingMode;
// 1: Close2GL CPU path — vPosition.xyz = NDC (clip.xyz/clip.w); vPosition.w = clip.w.
uniform int uClose2GlCpuClipVertex;
uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
uniform float uPointSize;
uniform vec3 uColor;
uniform vec3 uLightPosEye;
uniform vec3 uLightColor;
uniform float uAmbient;
uniform float uKd;
uniform float uKs;
uniform float uShininess;

in vec4 vPosition;
in vec3 vNormal;
in vec2 vTexCoord;

out vec3 vAmbientDiffuse;
out vec3 vSpecular;
out vec3 vNormalEye;
out vec3 vPosEye;
out vec2 fTexCoord;

void main()
{
    vec4 posEye;
    mat4 modelView = uView * uModel;
    mat3 normalMatrix = mat3(transpose(inverse(modelView)));

    if (uClose2GlCpuClipVertex != 0) {
        vec4 clipPos = vec4(vPosition.xyz * vPosition.w, vPosition.w);
        posEye = inverse(uProjection) * clipPos;
        gl_Position = clipPos;
    } else {
        posEye = modelView * vec4(vPosition.xyz, 1.0);
        gl_Position = uProjection * posEye;
    }
    vPosEye = posEye.xyz;
    vNormalEye = normalize(normalMatrix * vNormal);
    fTexCoord = vTexCoord;

    vSpecular = vec3(0.0);
    if (uShadingMode == 0) {
        vAmbientDiffuse = uColor;
    } else if (uShadingMode == 1 || uShadingMode == 2) {
        vec3 N = vNormalEye;
        vec3 L = normalize(uLightPosEye - vPosEye);
        float ndotl = max(dot(N, L), 0.0);
        vec3 ambient = uAmbient * uLightColor * uColor;
        vec3 diffuse = uKd * ndotl * uLightColor * uColor;
        vAmbientDiffuse = ambient + diffuse;
        if (uShadingMode == 2 && ndotl > 0.0) {
            vec3 V = normalize(-vPosEye);
            vec3 R = reflect(-L, N);
            float spec = pow(max(dot(R, V), 0.0), uShininess);
            vSpecular = uKs * spec * uLightColor;
        }
    } else {
        vAmbientDiffuse = uColor;
    }

    gl_PointSize = uPointSize;
}
