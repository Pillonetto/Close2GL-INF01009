#version 150 core

// 0 = flat, 1 = Gouraud A+D, 2 = Gouraud A+D+S, 3 = Phong (A+D+S in fragment)
uniform int uShadingMode;
// 1: Close2GL CPU path — vPosition.xyz = NDC (clip.xyz/clip.w); vPosition.w = clip.w.
uniform int uClose2GlCpuClipVertex;
uniform mat4 uModelView;
uniform mat4 uProjection;
uniform mat3 uNormalMatrix;
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

out vec3 vColor;
out vec3 vNormalEye;
out vec3 vPosEye;

void main()
{
    vec4 posEye;
    if (uClose2GlCpuClipVertex != 0) {
        // Rebuild homogeneous clip after CPU perspective divide (ndc * w, w).
         vec4 clipPos = vec4(vPosition.xyz * vPosition.w, vPosition.w);
        posEye = inverse(uProjection) * clipPos;
        gl_Position = clipPos;
    } else {
        posEye = uModelView * vec4(vPosition.xyz, 1.0);
        gl_Position = uProjection * posEye;
    }
    vPosEye = posEye.xyz;
    vNormalEye = normalize(uNormalMatrix * vNormal);

    if (uShadingMode == 0) {
        vColor = clamp(uColor, 0.0, 1.0);
    } else if (uShadingMode == 1 || uShadingMode == 2) {
        vec3 N = vNormalEye;
        vec3 L = normalize(uLightPosEye - vPosEye);
        float ndotl = max(dot(N, L), 0.0);
        vec3 ambient = uAmbient * uLightColor * uColor;
        vec3 diffuse = uKd * ndotl * uLightColor * uColor;
        vColor = ambient + diffuse;
        if (uShadingMode == 2) {
            vec3 V = normalize(-vPosEye);
            vec3 R = reflect(-L, N);
            float spec = pow(max(dot(R, V), 0.0), uShininess);
            vColor += uKs * spec * uLightColor;
        }
        vColor = clamp(vColor, 0.0, 1.0);
    } else {
        vColor = clamp(uColor, 0.0, 1.0);
    }

    gl_PointSize = uPointSize;
}
