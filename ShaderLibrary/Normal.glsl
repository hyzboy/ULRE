mat3 GetNormalMatrix()
{
    return mat3(camera.view*GetLocalToWorld());
}

vec3 GetNormal(mat3 normal_matrix,vec3 normal)
{
    return normalize(normal_matrix*normal);
}
