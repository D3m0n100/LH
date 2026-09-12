#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
LH-IDE：LH编译器可视化编译环境
嵌入式可组态软件平台 - 图形化前端

用法：
    python lh_ide.py

将此文件放在 E:\\lh-compiler\\ 目录下（与 lmc.py 同级）
"""

import sys
import os
import time
import threading
import traceback
from pathlib import Path
from datetime import datetime

# 添加项目路径
PROJECT_ROOT = Path(__file__).parent
sys.path.insert(0, str(PROJECT_ROOT / "src"))

import tkinter as tk
from tkinter import ttk, filedialog, scrolledtext, messagebox


# ──────────────────────────────────────────────
#  功能块加载器（从项目 registry 导入）
# ──────────────────────────────────────────────
def load_function_blocks():
    """加载所有功能块定义，返回按域分组的字典"""
    groups = {}
    try:
        from lh_compiler.function_blocks.definitions import get_all_definitions
        all_defs = get_all_definitions()
        for fb in all_defs:
            # 根据功能块名称的来源模块分组
            category = getattr(fb, 'category', None) or _guess_category(fb.name)
            if category not in groups:
                groups[category] = []
            groups[category].append(fb)
    except Exception as e:
        groups["加载失败"] = []
        print(f"功能块加载异常: {e}")
    return groups


def _guess_category(name):
    """根据功能块名称推断所属类别"""
    name_lower = name.lower()
    mapping = {
        "系统管理": ["system", "init", "watchdog"],
        "任务调度": ["task"],
        "逻辑控制": ["if", "for", "while", "case", "select", "switch"],
        "算术运算": ["add", "sub", "mul", "div", "mod", "abs", "sqrt", "math"],
        "比较运算": ["compare", "cmp", "eq", "ne", "gt", "lt", "ge", "le", "max", "min"],
        "I/O驱动": ["drv", "di", "do", "ai", "ao", "pwm", "io", "encoder"],
        "PID控制": ["pid", "ramp", "limit", "ff"],
        "定时计数": ["timer", "counter", "delay", "pulse", "ton", "tof"],
        "通信接口": ["sci", "spi", "can", "modbus", "comm", "uart"],
        "数据处理": ["data", "fifo", "stack", "buffer", "array"],
        "滤波处理": ["filter", "avg", "smooth"],
        "常量定义": ["const", "constant"],
        "显示输出": ["display", "led", "lcd"],
        "安全保护": ["safety", "protect", "alarm"],
        "参数管理": ["param", "config"],
        "应用功能": ["app", "application"],
        "挖掘控制": ["exca"],
        "TSO功能": ["tso"],
    }
    for cat, keywords in mapping.items():
        for kw in keywords:
            if kw in name_lower:
                return cat
    return "其他"


# ──────────────────────────────────────────────
#  编译器调用封装
# ──────────────────────────────────────────────
def run_compile(source_path, output_path=None, verbose=True, debug=False, grammar_path=None):
    """
    调用 LHCompiler 执行编译，返回 (success, log_text, output_file)
    """
    try:
        from lh_compiler.compiler import LHCompiler
        compiler = LHCompiler(
            verbose=verbose,
            debug=debug,
            grammar_path=grammar_path
        )
        result = compiler.compile_file(source_path, output_path)
        
        log_lines = []
        if result.success:
            log_lines.append(f"[成功] 编译完成: {result.output_file}")
            if hasattr(result, 'stats') and result.stats:
                for k, v in result.stats.items():
                    log_lines.append(f"  {k}: {v}")
        else:
            log_lines.append("[失败] 编译出错")
            for err in result.errors:
                log_lines.append(f"  {err}")
        
        # 捕获verbose输出（如果有的话）
        if hasattr(result, 'log') and result.log:
            log_lines.append("\n--- 详细日志 ---")
            log_lines.append(result.log)
        
        return result.success, "\n".join(log_lines), getattr(result, 'output_file', None)
    
    except Exception as e:
        tb = traceback.format_exc()
        return False, f"[异常] 编译器运行时错误:\n{e}\n\n{tb}", None


# ──────────────────────────────────────────────
#  主窗口
# ──────────────────────────────────────────────
class LmIdeApp:
    """LH-IDE 主应用程序"""

    TITLE = "LH-IDE  嵌入式可组态软件平台 v1.0"
    MIN_W, MIN_H = 1100, 700

    def __init__(self):
        self.root = tk.Tk()
        self.root.title(self.TITLE)
        self.root.geometry("1200x750")
        self.root.minsize(self.MIN_W, self.MIN_H)

        # 状态变量
        self.current_file = tk.StringVar(value="")
        self.compile_running = False
        self.fb_groups = {}

        self._build_menu()
        self._build_toolbar()
        self._build_main_area()
        self._build_statusbar()

        # 加载功能块
        self.root.after(200, self._load_fb_tree)

    # ─── 菜单 ───
    def _build_menu(self):
        menubar = tk.Menu(self.root)

        file_menu = tk.Menu(menubar, tearoff=0)
        file_menu.add_command(label="打开 LH 文件...", command=self._open_file, accelerator="Ctrl+O")
        file_menu.add_separator()
        file_menu.add_command(label="退出", command=self.root.quit)
        menubar.add_cascade(label="文件", menu=file_menu)

        compile_menu = tk.Menu(menubar, tearoff=0)
        compile_menu.add_command(label="编译", command=self._compile, accelerator="F5")
        compile_menu.add_command(label="编译(详细模式)", command=lambda: self._compile(verbose=True))
        compile_menu.add_command(label="编译(调试模式)", command=lambda: self._compile(verbose=True, debug=True))
        compile_menu.add_separator()
        compile_menu.add_command(label="批量编译...", command=self._batch_compile)
        menubar.add_cascade(label="编译", menu=compile_menu)

        help_menu = tk.Menu(menubar, tearoff=0)
        help_menu.add_command(label="关于", command=self._show_about)
        menubar.add_cascade(label="帮助", menu=help_menu)

        self.root.config(menu=menubar)
        self.root.bind("<Control-o>", lambda e: self._open_file())
        self.root.bind("<F5>", lambda e: self._compile())

    # ─── 工具栏 ───
    def _build_toolbar(self):
        tb = ttk.Frame(self.root, padding=(4, 2))
        tb.pack(side=tk.TOP, fill=tk.X)

        ttk.Button(tb, text="打开", command=self._open_file, width=8).pack(side=tk.LEFT, padx=2)
        ttk.Button(tb, text="编译 (F5)", command=self._compile, width=10).pack(side=tk.LEFT, padx=2)
        
        ttk.Separator(tb, orient=tk.VERTICAL).pack(side=tk.LEFT, fill=tk.Y, padx=6)
        
        ttk.Label(tb, text="当前文件:").pack(side=tk.LEFT, padx=(4, 0))
        file_label = ttk.Label(tb, textvariable=self.current_file, foreground="#0066CC", width=60, anchor=tk.W)
        file_label.pack(side=tk.LEFT, padx=4)

        # 编译选项
        self.var_verbose = tk.BooleanVar(value=True)
        self.var_debug = tk.BooleanVar(value=False)
        ttk.Checkbutton(tb, text="详细", variable=self.var_verbose).pack(side=tk.RIGHT, padx=2)
        ttk.Checkbutton(tb, text="调试", variable=self.var_debug).pack(side=tk.RIGHT, padx=2)

    # ─── 主区域（三栏布局）───
    def _build_main_area(self):
        main = ttk.PanedWindow(self.root, orient=tk.HORIZONTAL)
        main.pack(side=tk.TOP, fill=tk.BOTH, expand=True, padx=4, pady=2)

        # ── 左栏：选项卡（文件浏览器 + 功能块浏览器）──
        left_frame = ttk.Frame(main, padding=0)
        main.add(left_frame, weight=1)

        self.left_notebook = ttk.Notebook(left_frame)
        self.left_notebook.pack(fill=tk.BOTH, expand=True)

        # ── Tab1: 文件浏览器 ──
        file_tab = ttk.Frame(self.left_notebook, padding=4)
        self.left_notebook.add(file_tab, text="  文件浏览器  ")

        # 项目根目录选择
        dir_frame = ttk.Frame(file_tab)
        dir_frame.pack(fill=tk.X, pady=(0, 4))
        ttk.Label(dir_frame, text="目录:").pack(side=tk.LEFT)
        self.project_dir_var = tk.StringVar(value=str(PROJECT_ROOT / "lh_programs") if (PROJECT_ROOT / "lh_programs").is_dir() else str(PROJECT_ROOT))
        dir_entry = ttk.Entry(dir_frame, textvariable=self.project_dir_var, width=18)
        dir_entry.pack(side=tk.LEFT, fill=tk.X, expand=True, padx=4)
        ttk.Button(dir_frame, text="...", width=3, command=self._choose_project_dir).pack(side=tk.LEFT)

        # 文件树
        file_tree_frame = ttk.Frame(file_tab)
        file_tree_frame.pack(fill=tk.BOTH, expand=True)

        self.file_tree = ttk.Treeview(file_tree_frame, show="tree", selectmode="browse")
        self.file_tree.heading("#0", text="文件", anchor=tk.W)
        self.file_tree.column("#0", width=280, minwidth=150)

        file_tree_scroll = ttk.Scrollbar(file_tree_frame, orient=tk.VERTICAL, command=self.file_tree.yview)
        self.file_tree.configure(yscrollcommand=file_tree_scroll.set)
        self.file_tree.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        file_tree_scroll.pack(side=tk.RIGHT, fill=tk.Y)

        self.file_tree.bind("<Double-1>", self._on_file_double_click)

        # 刷新按钮
        ttk.Button(file_tab, text="刷新文件列表", command=self._refresh_file_tree).pack(fill=tk.X, pady=(4, 0))

        # 初始加载文件树
        self.root.after(300, self._refresh_file_tree)

        # ── Tab2: 功能块浏览器 ──
        fb_tab = ttk.Frame(self.left_notebook, padding=4)
        self.left_notebook.add(fb_tab, text="  功能块浏览器  ")

        # 搜索框
        search_frame = ttk.Frame(fb_tab)
        search_frame.pack(fill=tk.X, pady=(0, 4))
        ttk.Label(search_frame, text="搜索:").pack(side=tk.LEFT)
        self.fb_search_var = tk.StringVar()
        self.fb_search_var.trace_add("write", self._filter_fb_tree)
        search_entry = ttk.Entry(search_frame, textvariable=self.fb_search_var, width=20)
        search_entry.pack(side=tk.LEFT, fill=tk.X, expand=True, padx=4)

        # 树形控件
        tree_frame = ttk.Frame(fb_tab)
        tree_frame.pack(fill=tk.BOTH, expand=True)
        
        self.fb_tree = ttk.Treeview(tree_frame, columns=("type_id", "params"), show="tree headings", selectmode="browse")
        self.fb_tree.heading("#0", text="功能块", anchor=tk.W)
        self.fb_tree.heading("type_id", text="Type ID")
        self.fb_tree.heading("params", text="参数数量")
        self.fb_tree.column("#0", width=160, minwidth=100)
        self.fb_tree.column("type_id", width=70, minwidth=50, anchor=tk.CENTER)
        self.fb_tree.column("params", width=70, minwidth=50, anchor=tk.CENTER)
        
        fb_scroll = ttk.Scrollbar(tree_frame, orient=tk.VERTICAL, command=self.fb_tree.yview)
        self.fb_tree.configure(yscrollcommand=fb_scroll.set)
        self.fb_tree.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        fb_scroll.pack(side=tk.RIGHT, fill=tk.Y)

        self.fb_tree.bind("<<TreeviewSelect>>", self._on_fb_select)
        self.fb_tree.bind("<Double-1>", self._on_fb_double_click)

        # 功能块详情
        self.fb_detail = scrolledtext.ScrolledText(fb_tab, height=8, font=("Consolas", 9), state=tk.DISABLED, wrap=tk.WORD)
        self.fb_detail.pack(fill=tk.X, pady=(4, 0))

        # ── 中栏：代码编辑器 ──
        center_frame = ttk.LabelFrame(main, text="LH 源代码", padding=4)
        main.add(center_frame, weight=3)

        self.code_editor = scrolledtext.ScrolledText(
            center_frame, font=("Consolas", 11), wrap=tk.NONE,
            undo=True, bg="#FAFAFA", insertbackground="#333"
        )
        self.code_editor.pack(fill=tk.BOTH, expand=True)

        # 行号显示（简易）
        self.code_editor.bind("<KeyRelease>", self._update_line_info)
        self.code_editor.bind("<ButtonRelease>", self._update_line_info)

        # ── 右下方：编译输出日志 ──
        bottom_frame = ttk.LabelFrame(main, text="编译输出", padding=4)
        main.add(bottom_frame, weight=2)

        self.log_text = scrolledtext.ScrolledText(
            bottom_frame, font=("Consolas", 10), wrap=tk.WORD,
            state=tk.DISABLED, bg="#1E1E1E", fg="#D4D4D4",
            insertbackground="#FFF"
        )
        self.log_text.pack(fill=tk.BOTH, expand=True)

        # 日志标签颜色
        self.log_text.tag_configure("success", foreground="#4EC9B0")
        self.log_text.tag_configure("error", foreground="#F44747")
        self.log_text.tag_configure("warning", foreground="#CCA700")
        self.log_text.tag_configure("info", foreground="#9CDCFE")
        self.log_text.tag_configure("header", foreground="#569CD6", font=("Consolas", 10, "bold"))
        self.log_text.tag_configure("timestamp", foreground="#666666")

    # ─── 状态栏 ───
    def _build_statusbar(self):
        sb = ttk.Frame(self.root, padding=(4, 2))
        sb.pack(side=tk.BOTTOM, fill=tk.X)

        self.status_label = ttk.Label(sb, text="就绪", anchor=tk.W)
        self.status_label.pack(side=tk.LEFT, fill=tk.X, expand=True)

        self.line_info = ttk.Label(sb, text="行: 1  列: 1", width=16, anchor=tk.E)
        self.line_info.pack(side=tk.RIGHT)

        self.fb_count_label = ttk.Label(sb, text="功能块: 加载中...", width=20, anchor=tk.E)
        self.fb_count_label.pack(side=tk.RIGHT, padx=(0, 12))

    # ──────────── 文件浏览器操作 ────────────

    def _choose_project_dir(self):
        """选择项目根目录"""
        d = filedialog.askdirectory(title="选择项目目录", initialdir=self.project_dir_var.get())
        if d:
            self.project_dir_var.set(d)
            self._refresh_file_tree()

    def _refresh_file_tree(self):
        """扫描项目目录，显示 .lh 文件和子目录"""
        self.file_tree.delete(*self.file_tree.get_children())
        root_dir = Path(self.project_dir_var.get())
        if not root_dir.is_dir():
            return
        self._populate_dir(root_dir, "")
        self._set_status(f"已刷新文件列表: {root_dir}")

    def _populate_dir(self, dir_path, parent_node):
        """递归填充目录树，只显示含 .lh 文件的目录"""
        try:
            entries = sorted(dir_path.iterdir(), key=lambda p: (not p.is_dir(), p.name.lower()))
        except PermissionError:
            return

        for entry in entries:
            # 跳过隐藏目录和常见无关目录
            if entry.name.startswith('.') or entry.name in ('venv', '__pycache__', 'node_modules', '.git', '.antlr'):
                continue

            if entry.is_dir():
                # 检查目录下是否有 .lh 文件（递归）
                has_lm = any(entry.rglob("*.lh"))
                if has_lm:
                    node = self.file_tree.insert(parent_node, tk.END, text=entry.name + "/",
                                                  values=(str(entry),), open=True, tags=("dir",))
                    self._populate_dir(entry, node)

            elif entry.suffix.lower() == '.lh':
                self.file_tree.insert(parent_node, tk.END, text=entry.name,
                                       values=(str(entry),), tags=("lm_file",))

    def _on_file_double_click(self, event):
        """双击文件树中的 .lh 文件，打开到编辑器"""
        sel = self.file_tree.selection()
        if not sel:
            return
        item = sel[0]
        tags = self.file_tree.item(item, "tags")
        if "lm_file" not in tags:
            return  # 双击的是目录，不处理

        values = self.file_tree.item(item, "values")
        if not values:
            return
        fpath = values[0]

        if not os.path.isfile(fpath):
            return

        self.current_file.set(fpath)
        try:
            with open(fpath, "r", encoding="utf-8") as f:
                content = f.read()
            self.code_editor.delete("1.0", tk.END)
            self.code_editor.insert("1.0", content)
            self._set_status(f"已打开: {fpath}")
            self.root.title(f"{self.TITLE}  -  {Path(fpath).name}")
        except Exception as e:
            messagebox.showerror("打开失败", f"无法读取文件:\n{e}")

    def _on_fb_double_click(self, event):
        """双击功能块，将调用模板插入编辑器光标处"""
        sel = self.fb_tree.selection()
        if not sel:
            return
        item = sel[0]

        # 跳过分类节点（只有功能块叶节点才插入）
        tags = self.fb_tree.item(item, "tags")
        if "fb" not in tags:
            return

        fb_name = self.fb_tree.item(item, "text")

        # 查找对应功能块定义
        fb = None
        for blocks in self.fb_groups.values():
            for b in blocks:
                if b.name == fb_name:
                    fb = b
                    break
            if fb:
                break

        if not fb:
            return

        # ── 生成调用模板 ──
        lines = []

        # 输出变量注释行（方便用户知道有哪些输出）
        if hasattr(fb, 'outputs') and fb.outputs:
            out_names = []
            for pp in fb.outputs:
                pname = pp.name if hasattr(pp, 'name') else pp.get('name', '?')
                ptype = pp.type_name if hasattr(pp, 'type_name') else pp.get('type', '')
                out_names.append(f"{pname}:{ptype}" if ptype else pname)
            lines.append(f"// 输出: {', '.join(out_names)}")

        # 调用语句
        if hasattr(fb, 'inputs') and fb.inputs:
            params = []
            for pp in fb.inputs:
                pname = pp.name if hasattr(pp, 'name') else pp.get('name', '?')
                params.append(f"{pname} := ")
            lines.append(f"{fb.name}({', '.join(params)});")
        else:
            # 无输入参数的功能块
            lines.append(f"{fb.name}();")

        template = "\n".join(lines) + "\n"

        # 插入到编辑器光标处，切换焦点到编辑器
        self.code_editor.insert(tk.INSERT, template)
        self.code_editor.focus_set()
        self._set_status(f"已插入功能块模板: {fb.name}")

    # ──────────── 功能块树操作 ────────────

    def _load_fb_tree(self):
        """后台加载功能块定义并填充树"""
        self._set_status("正在加载功能块定义...")
        
        def _do_load():
            self.fb_groups = load_function_blocks()
            self.root.after(0, self._populate_fb_tree)
        
        threading.Thread(target=_do_load, daemon=True).start()

    def _populate_fb_tree(self, filter_text=""):
        """用功能块数据填充树形控件"""
        self.fb_tree.delete(*self.fb_tree.get_children())
        total = 0
        filter_lower = filter_text.lower()

        for category in sorted(self.fb_groups.keys()):
            blocks = self.fb_groups[category]
            if filter_lower:
                blocks = [b for b in blocks if filter_lower in b.name.lower()]
            if not blocks:
                continue

            cat_node = self.fb_tree.insert("", tk.END, text=f"{category} ({len(blocks)})", open=bool(filter_lower))
            for fb in sorted(blocks, key=lambda b: b.name):
                type_id_str = f"0x{fb.type_id:04X}" if hasattr(fb, 'type_id') and fb.type_id else "—"
                n_inputs = len(fb.inputs) if hasattr(fb, 'inputs') and fb.inputs else 0
                n_outputs = len(fb.outputs) if hasattr(fb, 'outputs') and fb.outputs else 0
                param_str = f"{n_inputs}入/{n_outputs}出"
                self.fb_tree.insert(cat_node, tk.END, text=fb.name, values=(type_id_str, param_str), tags=("fb",))
                total += 1

        self.fb_count_label.config(text=f"功能块: {total} 个")
        self._set_status(f"已加载 {total} 个功能块定义")

    def _filter_fb_tree(self, *args):
        self._populate_fb_tree(self.fb_search_var.get())

    def _on_fb_select(self, event):
        """选中功能块时显示详情"""
        sel = self.fb_tree.selection()
        if not sel:
            return
        item = sel[0]
        fb_name = self.fb_tree.item(item, "text")

        # 查找对应功能块
        fb = None
        for blocks in self.fb_groups.values():
            for b in blocks:
                if b.name == fb_name:
                    fb = b
                    break
            if fb:
                break

        if not fb:
            return

        # 构建详情文本
        lines = [f"功能块: {fb.name}"]
        if hasattr(fb, 'type_id') and fb.type_id:
            lines.append(f"Type ID: 0x{fb.type_id:04X} ({fb.type_id})")
        if hasattr(fb, 'memory_size'):
            lines.append(f"内存占用: {fb.memory_size} 字 ({fb.memory_size * 2} 字节)")
        
        if hasattr(fb, 'inputs') and fb.inputs:
            lines.append(f"\n输入参数 ({len(fb.inputs)}):")
            for p in fb.inputs:
                pname = p.name if hasattr(p, 'name') else p.get('name', '?')
                ptype = p.type_name if hasattr(p, 'type_name') else p.get('type', '?')
                lines.append(f"  {pname}: {ptype}")

        if hasattr(fb, 'outputs') and fb.outputs:
            lines.append(f"\n输出参数 ({len(fb.outputs)}):")
            for p in fb.outputs:
                pname = p.name if hasattr(p, 'name') else p.get('name', '?')
                ptype = p.type_name if hasattr(p, 'type_name') else p.get('type', '?')
                lines.append(f"  {pname}: {ptype}")

        # 生成调用模板
        if hasattr(fb, 'inputs') and fb.inputs:
            lines.append(f"\n调用模板:")
            params = []
            for pp in fb.inputs:
                pname = pp.name if hasattr(pp, 'name') else pp.get('name', '?')
                params.append(f"{pname} := ???")
            lines.append(f"  {fb.name}({', '.join(params)});")

        self.fb_detail.config(state=tk.NORMAL)
        self.fb_detail.delete("1.0", tk.END)
        self.fb_detail.insert(tk.END, "\n".join(lines))
        self.fb_detail.config(state=tk.DISABLED)

    # ──────────── 文件操作 ────────────

    def _open_file(self):
        """打开 .lh 文件"""
        fpath = filedialog.askopenfilename(
            title="选择 LH 源文件",
            filetypes=[("LH源文件", "*.lh"), ("所有文件", "*.*")],
            initialdir=str(PROJECT_ROOT)
        )
        if not fpath:
            return
        self.current_file.set(fpath)
        try:
            with open(fpath, "r", encoding="utf-8") as f:
                content = f.read()
            self.code_editor.delete("1.0", tk.END)
            self.code_editor.insert("1.0", content)
            self._set_status(f"已打开: {fpath}")
            self.root.title(f"{self.TITLE}  -  {Path(fpath).name}")
        except Exception as e:
            messagebox.showerror("打开失败", f"无法读取文件:\n{e}")

    # ──────────── 编译操作 ────────────

    def _compile(self, verbose=None, debug=None):
        """编译当前文件"""
        source = self.current_file.get()
        if not source or not os.path.isfile(source):
            messagebox.showwarning("提示", "请先打开一个 .lh 源文件")
            return

        if self.compile_running:
            return

        if verbose is None:
            verbose = self.var_verbose.get()
        if debug is None:
            debug = self.var_debug.get()

        self.compile_running = True
        self._set_status("编译中...")
        self._log_clear()
        self._log(f"═══ LH 编译器 ═══", "header")
        self._log(f"[{datetime.now().strftime('%H:%M:%S')}] 开始编译: {source}", "timestamp")
        self._log("")

        def _do_compile():
            t0 = time.time()
            # 保存编辑器内容到文件（如果有修改）
            try:
                editor_content = self.code_editor.get("1.0", tk.END).rstrip()
                with open(source, "r", encoding="utf-8") as f:
                    file_content = f.read().rstrip()
                if editor_content != file_content:
                    with open(source, "w", encoding="utf-8") as f:
                        f.write(editor_content)
                    self.root.after(0, lambda: self._log("[信息] 已保存编辑器内容到源文件", "info"))
            except Exception:
                pass

            success, log, output_file = run_compile(
                source, verbose=verbose, debug=debug
            )
            elapsed = time.time() - t0

            def _update_ui():
                self._log(log, "success" if success else "error")
                self._log("")
                self._log(f"[{datetime.now().strftime('%H:%M:%S')}] 耗时: {elapsed:.2f}s", "timestamp")

                if success:
                    self._log(f"输出文件: {output_file}", "success")
                    self._set_status(f"编译成功  ({elapsed:.2f}s)  ->  {output_file}")
                else:
                    self._set_status(f"编译失败  ({elapsed:.2f}s)")

                self.compile_running = False

            self.root.after(0, _update_ui)

        threading.Thread(target=_do_compile, daemon=True).start()

    def _batch_compile(self):
        """批量编译"""
        files = filedialog.askopenfilenames(
            title="选择多个 LH 源文件",
            filetypes=[("LH源文件", "*.lh"), ("所有文件", "*.*")],
            initialdir=str(PROJECT_ROOT)
        )
        if not files:
            return

        self._log_clear()
        self._log(f"═══ 批量编译 ({len(files)} 个文件) ═══", "header")

        def _do_batch():
            ok, fail = 0, 0
            for fpath in files:
                self.root.after(0, lambda f=fpath: self._log(f"\n编译: {f}", "info"))
                success, log, _ = run_compile(fpath, verbose=False)
                tag = "success" if success else "error"
                self.root.after(0, lambda l=log, t=tag: self._log(l, t))
                if success:
                    ok += 1
                else:
                    fail += 1

            def _done():
                self._log(f"\n═══ 批量编译完成: 成功 {ok}, 失败 {fail} ═══", "header")
                self._set_status(f"批量编译: {ok} 成功, {fail} 失败")

            self.root.after(0, _done)

        threading.Thread(target=_do_batch, daemon=True).start()

    # ──────────── 日志操作 ────────────

    def _log(self, text, tag=None):
        self.log_text.config(state=tk.NORMAL)
        self.log_text.insert(tk.END, text + "\n", tag)
        self.log_text.see(tk.END)
        self.log_text.config(state=tk.DISABLED)

    def _log_clear(self):
        self.log_text.config(state=tk.NORMAL)
        self.log_text.delete("1.0", tk.END)
        self.log_text.config(state=tk.DISABLED)

    # ──────────── 辅助 ────────────

    def _set_status(self, text):
        self.status_label.config(text=text)

    def _update_line_info(self, event=None):
        pos = self.code_editor.index(tk.INSERT)
        line, col = pos.split(".")
        self.line_info.config(text=f"行: {line}  列: {int(col) + 1}")

    def _show_about(self):
        messagebox.showinfo("关于 LH-IDE",
            "LH-IDE  嵌入式可组态软件平台 v1.0\n\n"
            "面向电液伺服阀控制系统的\n"
            "领域特定语言编译环境\n\n"
            "作者: 朱斌\n"
            "上海应用技术大学 轨道交通学院\n\n"
            "基于 ANTLR4 + Python + Tkinter"
        )

    def run(self):
        self.root.mainloop()


# ──────────────────────────────────────────────
#  入口
# ──────────────────────────────────────────────
if __name__ == "__main__":
    app = LmIdeApp()
    app.run()
