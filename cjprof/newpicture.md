为了更好地分析仓颉堆内存快照，cjprof 新增以下功能：

使用 cjprof heap -i xxx.data --dump-report 时，输出一张 svg 格式的图片，规格如下：
1、图片名为 xxx.svg，和堆内存快照文件名一致
2、图片中每个节点表示一个类型（即 class name），节点中标识类型、属于该类型的实例对象的数量、shallow size
3、节点 A 存在指向 节点 B 的边表示存在 A 类型的对象引用了 B 类型的对象
4、边上标识数量（该数量表示引用了多少 B 类型的对象）、shallow size（引用的 B 类型对象的 shallow size）
5、算法尽量高效

待讨论点：
1、节点中是否需要标识 retained size？如果节点存在 10 个实例对象，那节点的 retained size 是把这 10 个实例对象的 retained size 相加？这样是否合理？

澄清：
1. 输出路径与输入 .data 文件同目录
2. 
a) 假设 A 有 10 个实例，其中 3 个实例各自引用了若干个 B 对象（共引用 50 个 B 实例），那么数量是 50
b) shallow size 是被引用的所有 B 实例的 shallow size 总和

3. 暂时不考虑截断
4. 可以有自环边，可以处理循环引用
5. svg 是 --dump-report 的新增附件
6. 暂时不展示 retained size
7. 用不同颜色表示不同类型的根对象