# AssetPath

具体源代码参见CMAssetManage中的AssetPath.h

# 大致规则 

Asset代表的资产，意味应用程序本身所拥有的资源。而AssetPath意味指向这个资产的一个字符串。

AssetPath的组成规和Windows/UNIX/Linux的路径原则类似，大致如下：

```C++
LOCATION:/abc/123/test_material.mtl           
```

LOCATION  它代表资产所在的大范围位置，是可以不存在的，也就是说如下的写法也是可以的
```C++
:/abc/123/test_material.mtl
```
这个LOCATION的定义我们暂时有以下几个：

| LOCATION | 意义 |
|----------|------------------|
| 不写     | 应用程序本身的资产包 |
| Asset    | 应用程序本身的资产包 |
| Engine   | 代表引擎资产 |
| PlugIn   | 代表插件资产 |
| ExtPack  | 应用程序扩展资产包(一般用于额外安装或下载的资产包) |
| OS       | 操作系统真实路径访问 |
