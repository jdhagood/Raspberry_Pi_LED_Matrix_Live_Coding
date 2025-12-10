// Classic plasma
float hash(vec2 p) {
    p = fract(p * vec2(123.34, 345.45));
    p += dot(p, p + 34.345);
    return fract(p.x * p.y);
}

void main() {
    vec2 uv = (v_uv - 0.5) * 2.0;
    float t = u_time * 0.3;

    float v = 0.0;
    v += sin(uv.x * 3.0 + t);
    v += sin(uv.y * 4.0 - t * 1.3);
    v += sin((uv.x + uv.y) * 5.0 + t * 0.7);
    v /= 3.0;

    vec3 col = 0.5 + 0.5 * cos(6.28318 * (v + vec3(0.0, 0.33, 0.67)));

    gl_FragColor = vec4(col, 1.0); 
}