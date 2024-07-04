# 为什么我们使用INI/TOML做为资产文件格式?

    大部分的数据文件，我们会将其分为属性文本部分与二进制数据部分。

    其中属性部分，我们会使用INI/TOML方式编写。这样利于git/hg/svn等版本控制系统进行差异合并。

    比如一个纹理贴图，它的二进制部分首先是有它的“原始位图数据阵列”，它的属性部分包含“宽、高、象素格式”这样的不可更改数据，也包括“sRGB/Linear色彩空间、纹理分组、建议过滤方式、特定平台建议的压缩算法”等可调整数据。还有资产使用过程中产生的“引用资产列表”和“被引用资产列表”。

    而这些可调整数据，我们可能是会随时修改的，甚至是不同的人可能会修改不同的部分。

    如果我们将这些数据，全部放入一个二进制文件中，比如png/jpeg中。那么每修改一个小小的属性，都需要向git/svn仓库提交整个二进制文件。这是相当愚蠢和不合理的。
    
    而众所周知：不管是免费的git/hg/svn，还是收费的p4，它们针对文本文件都会有一系列的差异化比较合并功能。而这个差异化自动合并，都是基于文本行的。
    所以，我们的所有资产文件，都会分裂成二进制数据文件和属性文本文件。而在这个属性文本文件中，对于每一项属性，是使用独立的行来保存数据的。


# Why do we use the INI/TOML as an asset file format?
    For most files, we will split into the attributes text part and binary data part.

    We will use the INI/TOML format to write the properties part. Because the format easily merges diff by git/hg/svn.

    For example a texture. It's binary data includes "Raw bitmap data".
    Its attributes part include "width, height, pixel format", they can't change.
    Also, "sRGB/Linear colour space", "texture group", "recommend filter type" and "recommend compress format of special platform".

    We may modify this data at any time. Even many people modify different parts at the same time.

    If we use a binary file that it includes all data. for example, .png or .jpeg.
    We need commit whole binary data after changed a few attributes. This behaviour is quite stupid and irrational.

    Most people know this knowledge: Free git/hg/svn and commercial p4. They both include a merge-diff tool, which is text based.
    so, we all asset files, they should split into the attributes text part and binary data part.
    In this attribute's text file, a separate line is used to save data for each attribute.
    