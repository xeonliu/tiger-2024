# Comments Handling

通过进入新的Start Condition，也就是教材所说的状态进行处理。

由于需要处理嵌套的注释，并且观察到框架代码中存在`comment_level_`成员，故在进入第一层注释后需要记录当前注释的深度，在碰到`*/`时降低深度，若为0,则退出注释状态。

# String Handling
要注意`adjustStr`和`adjust`的区别。字符串开始处应为开头双引号的位置，故只有`\"`可以`adjust`，然后进入`string`状态，里面的匹配只`adjustStr`。

然后注意需要维护一个Buffer，因为无法用单个正则表达式匹配整个字符串。

还需要注意使用`matched`获取当前正则表达式匹配的字符串，使用`setMatched`修改当前的匹配，然后通过`return`返回Token出去。

然后就是各种Escape Chracter的匹配，可以使用各种C/C++函数去处理。

# Error handling

```
errormsg_->Error(errormsg_->tok_pos_, "illegal token");
```

# End-of-file handling

No Special Handling.