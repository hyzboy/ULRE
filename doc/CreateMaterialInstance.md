# CreateMaterialInstance

## 1st

最早最根本的方法，直接在C++代码层面通过mtl::CreateVertexColor2D()函数来创建MaterialCreateInfo
```C++
mtl::Material2DCreateConfig cfg(GetDeviceAttribute(),"VertexColor2D",PrimitiveType::Triangles);

cfg.coordinate_system=CoordinateSystem2D::NDC;
cfg.local_to_world=false;

AutoDelete<mtl::MaterialCreateInfo> mci=mtl::CreateVertexColor2D(&cfg);

material_instance=CreateMaterialInstance(mci);
```

## 2nd
注册材质系统引入后的方法，通过名称"VertexColor2D"来创建MaterialCreateInfo
```C++
mtl::Material2DCreateConfig cfg(GetDeviceAttribute(),"VertexColor2D",PrimitiveType::Triangles);

cfg.coordinate_system=CoordinateSystem2D::NDC;
cfg.local_to_world=false;

AutoDelete<mtl::MaterialCreateInfo> mci=mtl::CreateMaterialCreateInfo("VertexColor2D",&cfg);

material_instance=CreateMaterialInstance(mci);
```

## 3rd
其实是第二种方法在WorkObject层面的封装
```C++
mtl::Material2DCreateConfig cfg(GetDeviceAttribute(),"VertexColor2D",PrimitiveType::Triangles);

cfg.coordinate_system=CoordinateSystem2D::NDC;
cfg.local_to_world=false;

material_instance=CreateMaterialInstance("VertexColor2D",&cfg);
```

## 4th
是更进一步的封装，通过材质配置文件连带Material2DCreateConfig的具体配置都进行了封闭。
```C++
AssetPath path(":/asset/test_material.mtl");

material_instance=CreateMaterialInstance(path);
```