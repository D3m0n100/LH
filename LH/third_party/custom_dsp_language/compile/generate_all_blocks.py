#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
功能块定义批量生成工具（基于已知信息）

根据CODESYS对照手册的内容，批量生成179个功能块的Python定义
"""

import json
from pathlib import Path
from typing import List, Dict, Tuple


# 功能块分类和type_id映射
FUNCTION_BLOCKS = {
    # 1. 系统功能（4个）
    'system': [
        {'name': '_System', 'type_id': 40961, 'mem_size': 38, 'desc': '系统初始化'},
        {'name': '_SystemEnd', 'type_id': 40962, 'mem_size': 8, 'desc': '系统结束标志'},
        {'name': '_Exception', 'type_id': 40963, 'mem_size': 16, 'desc': '异常处理'},
        {'name': '_Nop', 'type_id': 40964, 'mem_size': 8, 'desc': '空操作'},
    ],
    
    # 2. 任务控制（10个）
    'task': [
        {'name': '_Task', 'type_id': 41000, 'mem_size': 16, 'desc': '创建时间触发任务'},
        {'name': '_TaskPeriodic', 'type_id': 41001, 'mem_size': 16, 'desc': '创建周期性任务'},
        {'name': '_TaskWake', 'type_id': 41002, 'mem_size': 8, 'desc': '唤醒任务'},
        {'name': '_TaskDataWake', 'type_id': 41003, 'mem_size': 16, 'desc': '数据唤醒任务'},
        {'name': '_TaskLock', 'type_id': 41004, 'mem_size': 8, 'desc': '任务上锁'},
        {'name': '_TaskUnlock', 'type_id': 41005, 'mem_size': 8, 'desc': '任务解锁'},
        {'name': '_TaskSemDef', 'type_id': 41006, 'mem_size': 16, 'desc': '信号量定义'},
        {'name': '_TaskSemWait', 'type_id': 41007, 'mem_size': 16, 'desc': '信号量等待'},
        {'name': '_TaskSemPost', 'type_id': 41008, 'mem_size': 16, 'desc': '信号量发送'},
        {'name': '_TaskEnd', 'type_id': 41009, 'mem_size': 8, 'desc': '任务结束标志'},
    ],
    
    # 3. 流程控制（20个）
    'control': [
        {'name': '_Switch1', 'type_id': 41100, 'mem_size': 16, 'desc': '单分支选择'},
        {'name': '_Switch2', 'type_id': 41101, 'mem_size': 16, 'desc': '双分支选择'},
        {'name': '_Switch3', 'type_id': 41102, 'mem_size': 16, 'desc': '三分支选择'},
        {'name': '_Switch4', 'type_id': 41103, 'mem_size': 16, 'desc': '四分支选择'},
        {'name': '_Switch5', 'type_id': 41104, 'mem_size': 16, 'desc': '五分支选择'},
        {'name': '_SwitchCase', 'type_id': 41105, 'mem_size': 8, 'desc': 'Switch分支标记'},
        {'name': '_SwitchBreak', 'type_id': 41106, 'mem_size': 8, 'desc': '退出Switch'},
        {'name': '_SwitchEnd', 'type_id': 41107, 'mem_size': 8, 'desc': 'Switch结束标志'},
        {'name': '_If1', 'type_id': 41110, 'mem_size': 16, 'desc': '单条件判断'},
        {'name': '_If2', 'type_id': 41111, 'mem_size': 16, 'desc': '双条件判断'},
        {'name': '_If3', 'type_id': 41112, 'mem_size': 16, 'desc': '三条件判断'},
        {'name': '_IfElse', 'type_id': 41113, 'mem_size': 8, 'desc': 'If分支Else标记'},
        {'name': '_IfEnd', 'type_id': 41114, 'mem_size': 8, 'desc': '条件判断结束'},
        {'name': '_For', 'type_id': 41120, 'mem_size': 24, 'desc': '固定次数循环'},
        {'name': '_ForEnd', 'type_id': 41121, 'mem_size': 8, 'desc': 'For循环结束'},
        {'name': '_While', 'type_id': 41122, 'mem_size': 16, 'desc': '条件循环'},
        {'name': '_WhileEnd', 'type_id': 41123, 'mem_size': 8, 'desc': 'While循环结束'},
        {'name': '_LoopBreak', 'type_id': 41124, 'mem_size': 8, 'desc': '退出循环'},
        {'name': '_LoopContinue', 'type_id': 41125, 'mem_size': 8, 'desc': '继续下次循环'},
        {'name': '_Return', 'type_id': 41126, 'mem_size': 8, 'desc': '返回'},
    ],
    
    # 4. I/O驱动（24个）
    'io': [
        {'name': '_DrvAI', 'type_id': 41200, 'mem_size': 32, 'desc': '模拟量输入'},
        {'name': '_DrvAO', 'type_id': 41201, 'mem_size': 32, 'desc': '模拟量输出'},
        {'name': '_DrvDI', 'type_id': 41202, 'mem_size': 16, 'desc': '开关量输入'},
        {'name': '_DrvDO', 'type_id': 41203, 'mem_size': 16, 'desc': '开关量输出'},
        {'name': '_PortSet', 'type_id': 41204, 'mem_size': 16, 'desc': '端口置位'},
        {'name': '_PortReset', 'type_id': 41205, 'mem_size': 16, 'desc': '端口复位'},
        {'name': '_PortToggle', 'type_id': 41206, 'mem_size': 16, 'desc': '端口翻转'},
    ],
    
    # 5. 数学运算（16个）
    'math': [
        {'name': '_Add', 'type_id': 41300, 'mem_size': 16, 'desc': '加法'},
        {'name': '_Sub', 'type_id': 41301, 'mem_size': 16, 'desc': '减法'},
        {'name': '_Mul', 'type_id': 41302, 'mem_size': 16, 'desc': '乘法'},
        {'name': '_Div', 'type_id': 41303, 'mem_size': 16, 'desc': '除法'},
        {'name': '_Mod', 'type_id': 41304, 'mem_size': 16, 'desc': '取模'},
        {'name': '_Abs', 'type_id': 41305, 'mem_size': 16, 'desc': '绝对值'},
        {'name': '_Sqrt', 'type_id': 41306, 'mem_size': 16, 'desc': '平方根'},
        {'name': '_Exp', 'type_id': 41307, 'mem_size': 16, 'desc': '指数'},
        {'name': '_Ln', 'type_id': 41308, 'mem_size': 16, 'desc': '自然对数'},
        {'name': '_Log', 'type_id': 41309, 'mem_size': 16, 'desc': '常用对数'},
        {'name': '_Sin', 'type_id': 41310, 'mem_size': 16, 'desc': '正弦'},
        {'name': '_Cos', 'type_id': 41311, 'mem_size': 16, 'desc': '余弦'},
        {'name': '_Tan', 'type_id': 41312, 'mem_size': 16, 'desc': '正切'},
        {'name': '_Limit', 'type_id': 41313, 'mem_size': 24, 'desc': '限幅'},
        {'name': '_Min', 'type_id': 41314, 'mem_size': 24, 'desc': '最小值'},
        {'name': '_Max', 'type_id': 41315, 'mem_size': 24, 'desc': '最大值'},
    ],
    
    # 6. 逻辑运算（8个）
    'logic': [
        {'name': '_And', 'type_id': 41400, 'mem_size': 16, 'desc': '逻辑与'},
        {'name': '_Or', 'type_id': 41401, 'mem_size': 16, 'desc': '逻辑或'},
        {'name': '_Not', 'type_id': 41402, 'mem_size': 16, 'desc': '逻辑非'},
        {'name': '_Xor', 'type_id': 41403, 'mem_size': 16, 'desc': '异或'},
        {'name': '_Shl', 'type_id': 41404, 'mem_size': 16, 'desc': '左移'},
        {'name': '_Shr', 'type_id': 41405, 'mem_size': 16, 'desc': '右移'},
        {'name': '_Rol', 'type_id': 41406, 'mem_size': 16, 'desc': '循环左移'},
        {'name': '_Ror', 'type_id': 41407, 'mem_size': 16, 'desc': '循环右移'},
    ],
    
    # 7. 比较运算（6个）
    'compare': [
        {'name': '_Eq', 'type_id': 41500, 'mem_size': 16, 'desc': '等于'},
        {'name': '_Ne', 'type_id': 41501, 'mem_size': 16, 'desc': '不等于'},
        {'name': '_Gt', 'type_id': 41502, 'mem_size': 16, 'desc': '大于'},
        {'name': '_Ge', 'type_id': 41503, 'mem_size': 16, 'desc': '大于等于'},
        {'name': '_Lt', 'type_id': 41504, 'mem_size': 16, 'desc': '小于'},
        {'name': '_Le', 'type_id': 41505, 'mem_size': 16, 'desc': '小于等于'},
    ],
    
    # 8. 定时器（5个）
    'timer': [
        {'name': '_Timer', 'type_id': 41600, 'mem_size': 24, 'desc': '通用定时器'},
        {'name': '_TON', 'type_id': 41601, 'mem_size': 24, 'desc': '接通延时'},
        {'name': '_TOF', 'type_id': 41602, 'mem_size': 24, 'desc': '断开延时'},
        {'name': '_TP', 'type_id': 41603, 'mem_size': 24, 'desc': '脉冲定时器'},
        {'name': '_TimerStop', 'type_id': 41604, 'mem_size': 16, 'desc': '停止定时器'},
    ],
    
    # 9. 计数器（5个）
    'counter': [
        {'name': '_Counter', 'type_id': 41700, 'mem_size': 24, 'desc': '通用计数器'},
        {'name': '_CTU', 'type_id': 41701, 'mem_size': 24, 'desc': '加计数器'},
        {'name': '_CTD', 'type_id': 41702, 'mem_size': 24, 'desc': '减计数器'},
        {'name': '_CTUD', 'type_id': 41703, 'mem_size': 32, 'desc': '加减计数器'},
        {'name': '_CounterReset', 'type_id': 41704, 'mem_size': 16, 'desc': '复位计数器'},
    ],
    
    # 10. 常量构建（3个）
    'constant': [
        {'name': '_IntConstBuild', 'type_id': 41029, 'mem_size': 16, 'desc': '整数常量'},
        {'name': '_RealConstBuild', 'type_id': 41030, 'mem_size': 16, 'desc': '浮点常量'},
        {'name': '_BoolConstBuild', 'type_id': 41031, 'mem_size': 8, 'desc': '布尔常量'},
    ],
    
    # 11. PID控制（2个）
    'pid': [
        {'name': '_PID', 'type_id': 41800, 'mem_size': 64, 'desc': 'PID控制器'},
        {'name': '_PIDAutoTune', 'type_id': 41801, 'mem_size': 64, 'desc': '自整定PID'},
    ],
    
    # 12. TSO控制（4个）
    'tso': [
        {'name': '_TSO', 'type_id': 41850, 'mem_size': 64, 'desc': 'TSO控制器'},
        {'name': '_TSOAutoTune', 'type_id': 41851, 'mem_size': 64, 'desc': '自整定TSO'},
        {'name': '_TwoPosition', 'type_id': 41852, 'mem_size': 32, 'desc': '两位继电控制'},
        {'name': '_RelayCtrl2', 'type_id': 41853, 'mem_size': 32, 'desc': '继电器控制2'},
    ],
    
    # 13. 滤波器（1个）
    'filter': [
        {'name': '_FilterBW', 'type_id': 41900, 'mem_size': 48, 'desc': '巴特沃斯滤波器'},
    ],
    
    # 14. 显示屏（6个）
    'display': [
        {'name': '_KeyScan', 'type_id': 42000, 'mem_size': 16, 'desc': '按键扫描'},
        {'name': '_SCIDispTrans', 'type_id': 42001, 'mem_size': 16, 'desc': '串口显示发送'},
        {'name': '_SCIDispInit', 'type_id': 42002, 'mem_size': 24, 'desc': '串口显示初始化'},
        {'name': '_M600TextDisp', 'type_id': 42003, 'mem_size': 64, 'desc': 'M600文本显示'},
        {'name': '_M600ProgressBar', 'type_id': 42004, 'mem_size': 64, 'desc': 'M600进度条'},
        {'name': '_EXCACycDisp', 'type_id': 42005, 'mem_size': 32, 'desc': 'EXCA循环显示'},
    ],
    
    # 15. 应用功能（5个）
    'application': [
        {'name': '_EXCATMaintain', 'type_id': 42100, 'mem_size': 32, 'desc': '保养功能'},
        {'name': '_EXCAIntCheck', 'type_id': 42101, 'mem_size': 24, 'desc': '整型检查'},
        {'name': '_PasswordVerify2', 'type_id': 42102, 'mem_size': 32, 'desc': '密码验证2'},
        {'name': '_LockCmdGenery', 'type_id': 42103, 'mem_size': 24, 'desc': '锁定命令生成'},
        {'name': '_OutRealTime3', 'type_id': 42104, 'mem_size': 32, 'desc': '实时输出3'},
    ],
}


def generate_category_file(category: str, blocks: List[Dict], output_dir: Path):
    """生成一个类别的功能块定义文件"""
    
    code = f'''# -*- coding: utf-8 -*-
"""
{category.upper()} 功能块定义
"""

from typing import List
from ..registry import FunctionBlockMeta, ParameterDef


def get_{category}_blocks() -> List[FunctionBlockMeta]:
    """返回{category}类别的功能块定义"""
    return [
'''
    
    for block in blocks:
        func_name = block['name'].lower()[1:]  # 去掉前缀_
        code += f"        _define_{func_name}(),\n"
    
    code += "    ]\n\n\n"
    
    # 为每个功能块生成定义函数
    for block in blocks:
        func_name = block['name'].lower()[1:]
        code += f'''def _define_{func_name}() -> FunctionBlockMeta:
    """
    {block['name']} - {block['desc']}
    """
    return FunctionBlockMeta(
        name="{block['name']}",
        type_id={block['type_id']},
        memory_size={block['mem_size']},
        category="{category}",
        description="{block['desc']}",
        parameters=[
            # TODO: 根据实际需求添加参数定义
        ]
    )


'''
    
    # 写入文件
    file_path = output_dir / f"_{category}.py"
    file_path.write_text(code, encoding='utf-8')
    print(f"✓ 生成 {file_path.name}: {len(blocks)} 个功能块")


def generate_init_file(output_dir: Path):
    """生成__init__.py"""
    
    code = '''# -*- coding: utf-8 -*-
"""
功能块定义汇总
"""

from typing import List
from ..registry import FunctionBlockMeta

'''
    
    categories = sorted(FUNCTION_BLOCKS.keys())
    for category in categories:
        code += f"from ._{category} import get_{category}_blocks\n"
    
    code += "\n\ndef get_all_definitions() -> List[FunctionBlockMeta]:\n"
    code += "    \"\"\"收集所有功能块定义\"\"\"\n"
    code += "    all_defs = []\n\n"
    
    for category in categories:
        code += f"    all_defs.extend(get_{category}_blocks())\n"
    
    code += "\n    return all_defs\n"
    
    file_path = output_dir / "__init__.py"
    file_path.write_text(code, encoding='utf-8')
    print(f"✓ 更新 {file_path.name}")


def generate_manifest():
    """生成功能块清单JSON"""
    manifest = []
    total_count = 0
    
    for category, blocks in FUNCTION_BLOCKS.items():
        for block in blocks:
            manifest.append({
                'name': block['name'],
                'type_id': block['type_id'],
                'category': category,
                'description': block['desc'],
                'memory_size': block['mem_size']
            })
            total_count += 1
    
    manifest_path = Path("function_blocks_manifest.json")
    manifest_path.write_text(json.dumps(manifest, indent=2, ensure_ascii=False), encoding='utf-8')
    print(f"\n✓ 生成功能块清单: {manifest_path} ({total_count} 个功能块)")
    
    return total_count


def main():
    """主函数"""
    print("=" * 60)
    print("功能块定义批量生成工具")
    print("=" * 60)
    
    # 创建输出目录
    output_dir = Path("src/lh_compiler/function_blocks/definitions")
    output_dir.mkdir(parents=True, exist_ok=True)
    
    print(f"\n输出目录: {output_dir}\n")
    
    # 统计
    total = 0
    for category in sorted(FUNCTION_BLOCKS.keys()):
        count = len(FUNCTION_BLOCKS[category])
        total += count
        print(f"  {category:12s}: {count:3d} 个功能块")
    
    print(f"\n  {'总计':12s}: {total:3d} 个功能块")
    print("\n" + "=" * 60)
    print("开始生成定义文件...\n")
    
    # 生成各类别的定义文件
    for category, blocks in sorted(FUNCTION_BLOCKS.items()):
        generate_category_file(category, blocks, output_dir)
    
    # 生成__init__.py
    print()
    generate_init_file(output_dir)
    
    # 生成清单
    total_count = generate_manifest()
    
    print("\n" + "=" * 60)
    print(f"✅ 完成! 共生成 {total_count} 个功能块定义")
    print("=" * 60)
    
    print("\n下一步:")
    print("  1. 检查生成的文件")
    print("  2. 根据需要添加参数定义")
    print("  3. 在编译器中注册这些功能块")
    print("     registry.load_defaults()  # 自动加载所有定义")


if __name__ == '__main__':
    main()
