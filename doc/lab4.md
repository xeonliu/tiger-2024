Lab 4: Type Checking
Description
Write a type-checking phase for your compiler, a module semant describes this phase.

Related files for this lab are:
  • src/tiger/semant/semant.* The SemAnalyze function implementations of all AST classes

  • src/tiger/semant/type.* A series of Type classes describe types in tiger language.

  • src/tiger/env/env.* The EnvEntry classes which help compiler store the information of variables&functions when doing type checking.

To finish this lab, you will only need to fill in the file semant.cc. You can change the declarations of function SemAnalyze or modify any file as you want.

Notice: Before you start this lab, you should carefully read the chapter 5 of the textbook. And if you have any question about this lab, feel free to contact teaching assistants.

Important Notes
You should use member errormsg_ in ProgSem to report semantic errors you detect.

Environment
You will use the same code framework that you had set up when you worked on lab3 and use the code you written in lab3. What you need to do now is to pull the latest update of the code framework. You may have to do some code merging jobs. If you have any difficulties in merging codes, you can ask TAs in our Wechat group.

shell% git feth upstream
shell% git checkout -b lab4 upstream/lab4
shell% git push -u origin
shell% git merge lab3

You may have to do some code merging jobs here.

After merging, you are supposed to push your update to your remote repo
shell% git add files
shell% git commit -m "[lab4] merge lab3"
shell% git push origin lab4

Now you can check your pipeline on gitlab, if you successfully merge, you should see your previous labs passed.
Grade Test
The lab environment contains a grading script named as grade.sh, you can use it to evaluate your code, and that's how we grade your code, too. If you pass all the tests, the script will print a successful hint, otherwise, it will output some error messages. You can execute the script with the following commands.

shell% make gradelab4
shell% ...
shell% [^_^]: Pass Lab4 #If you pass all the tests, you will see these messages.
shell% TOTAL SCORE: 100
Handin
The deadline of this lab is on Friday 12:00 AT NOON, Nov 22, 2024, and if you miss the deadline, points will be deducted based on the number of days you are late!

After you have passed the grade test, you need first commit your modification, then push it to your remote repository on gitlab.You can use the following commands to finish this step.

shell% git add . 
shell% git commit -m "[lab4] finish lab4"
shell% git push origin lab4
In the end, you need to check your pipeline on gitlab. Make sure you see all the test passed, otherwise you may not get your score.

env.h

EnvEntry: Base Class

VarEntry: Type

FunEntry: Types of formals & results.

VEnv: `sym::Symbol`-> `env::EnvEntry`映射。
TEnv: `sym::Symbol` -> `type::Ty`映射

types.h

Type:
NilTy
IntTy
StringTy
VoidTy
RecordTy: {FieldList* fields_}
ArrayTy
NameTy
TyList

Field: name & type
FieldList

absyn.h
+ Var
+ 

Process: Parse elements in absyn.

Elements in Absyn holds
```
class Var;
class Exp;
class Dec;
class Ty;

class ExpList;
class FieldList;
class FunDecList;
class NameAndTyList;
class DecList;
class EFieldList;
```

and symbols from sym::Symbol(`%token <sym> ID`) in lab3.

Evaluate Symbol Type.

表达式评估不须改变环境。

Semantic Analysis:
Determine the type of each element in abstract syntax tree.
Var, Expression, etc...

