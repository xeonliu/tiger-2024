
Compilers 2024
SE3355 2024

Home / News
Schedule
General Information
Labs

Lab 6: Register Allocation
Description
Finish register allocation in your tiger compiler.

Related files for this lab are:
  • src/tiger/liveness/.* Files related to liveness analysis

  • src/tiger/regalloc/.* Files related to register allocation

To finish this lab, you will only need to finish the following modules: { liveness analysis } { register allocation } you can modify any file to finish your design.


In your generated code, you should do what you can to avoid unnecessary stack accesses (push and pop), which means placing as many variables as possible in registers. Otherwise, you will NOT get the full scores!

Notice: Before you start this lab, you should carefully read the chapter 10,11 of the textbook. And if you have any question about this lab, feel free to contact 吕泓涛, who is the teaching assistant responsible for lab 6.
Notice: Pay attention to the frame_info_map data structure defined and used in output.cc. At the end of the LLVM IR program output from lab5-1 (or LLVM reference), the final offset and frame size values for each function are recorded as comments, and these are read back into frame_info_map in lab5-2 and lab6. When you perform spilling in lab6, you need to modify the values in this data structure, allocate new positions on the stack frame, and ensure that your spill does not cause any issues with the stack structure. The framework will populate the stack frame sizes from frame_info_map into the output assembly. Further details can be found in output.cc.
Notice: Pay attention to the frame_info_map data structure defined and used in output.cc. At the end of the LLVM IR program output from lab5-1 (or LLVM reference), the final offset and frame size values for each function are recorded as comments, and these are read back into frame_info_map in lab5-2 and lab6. When you perform spilling in lab6, you need to modify the values in this data structure, allocate new positions on the stack frame, and ensure that your spill does not cause any issues with the stack structure. The framework will populate the stack frame sizes from frame_info_map into the output assembly. Further details can be found in output.cc.
Environment
You will use the same code framework that you had set up when you worked on lab6 and use the code you written in previous labs. What you need to do now is to pull the latest update of the code framework if there are any. You may have to do some code merging jobs. If you have any difficulties in merging codes, you can ask TAs in our Wechat group.

shell% git fetch upstream
shell% git checkout -b lab6 upstream/lab6
shell% git push -u origin
shell% git merge lab5-part2

shell% git add files
shell% git commit -m "[lab6] merge lab5-part2"
shell% git push origin lab6

If you haven't set it up before, you should follow the instructions here to set up your lab environment.

Grade Test
The lab environment contains a grading script named as grade.sh, you can use it to evaluate your code, and that's how we grade your code, too. If you pass all the tests, the script will print a successful hint, otherwise, it will output some error messages. You can execute the script with the following commands.

Remember grading your lab under docker or unix shell! Never run these commands under windows cmd.
shell% make gradelab6
shell% ...
shell% [^_^]: Pass #If you pass all the tests, you will see these messages.
shell% TOTAL SCORE: 100
If you wish to skip lab 5-1 and use the ref LLVM as the input LLVM IR for developing and testing lab 6, you can use gradelab6-llref for evaluation. You can achieve full credit by passing the evaluation in any way you choose.

shell% make gradelab6-llref
shell% ...
shell% [^_^]: Pass #If you pass all the tests, you will see these messages.
shell% TOTAL SCORE: 100
Handin
The deadline of this lab is on Tuesday 12:00 AT NOON, January 14, 2024, and if you miss the deadline, points will be deducted based on the number of days you are late!

After you have passed the grade test, you need first commit your modification, then push it to your remote repository on gitlab. You can use the following commands to finish this step.

shell% git add files 
shell% git commit -m "[lab6] finish lab6"
shell% git push origin lab6 
Go to Top // Compilers Home Page
Questions or comments regarding Compilers course? Send e-mail to the course Staffs or TAs.
Last updated: Wed Oct. 16 2019

# File Change

```
src/tiger/liveness
src/tiger/output
src/tiger/regalloc
src/tiger/output
```

`output.cc`

```diff
diff --git a/src/tiger/output/output.cc b/src/tiger/output/output.cc
index ba53717..4b6b6a3 100644
--- a/src/tiger/output/output.cc
+++ b/src/tiger/output/output.cc
@@ -116,6 +116,7 @@ namespace frame {
 void ProcFrag::OutputAssem(FILE *out, OutputPhase phase, bool need_ra) const {
   std::unique_ptr<canon::Traces> traces;
   std::unique_ptr<cg::AssemInstr> assem_instr;
+  std::unique_ptr<ra::Result> allocation;
 
   // When generating proc fragment, do not output string assembly
   if (phase != Proc)
@@ -149,16 +150,16 @@ void ProcFrag::OutputAssem(FILE *out, OutputPhase phase, bool need_ra) const {
 
   assem::InstrList *il = assem_instr.get()->GetInstrList();
 
-  // if (need_ra) {
-  //   // Lab 6: register allocation
-  //   TigerLog("----====Register allocate====-----\n");
-  //   ra::RegAllocator reg_allocator(body_->getName().str(),
-  //                                  std::move(assem_instr));
-  //   reg_allocator.RegAlloc();
-  //   allocation = reg_allocator.TransferResult();
-  //   il = allocation->il_;
-  //   color = temp::Map::LayerMap(reg_manager->temp_map_, allocation->coloring_);
-  // }
+  if (need_ra) {
+    // Lab 6: register allocation
+    TigerLog("----====Register allocate====-----\n");
+    ra::RegAllocator reg_allocator(body_->getName().str(),
+                                   std::move(assem_instr));
+    reg_allocator.RegAlloc();
+    allocation = reg_allocator.TransferResult();
+    il = allocation->il_;
+    color = temp::Map::LayerMap(reg_manager->temp_map_, allocation->coloring_);
+  }
 
   std::string proc_name = body_->getName().str();
```



用到rsp的都是operonstr

除法的目的寄存器

Store都是src