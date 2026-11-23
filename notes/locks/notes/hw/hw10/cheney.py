import time
from typing import List, Optional


class Obj:
    """模拟堆内存中的对象实体"""

    def __init__(self, name: str, fields: Optional[List["Obj"]] = None):
        self.name = name
        self.fields: List[Optional["Obj"]] = fields if fields is not None else []
        self.forwarded: Optional["Obj"] = None  # 转发指针
        self.forwarded_idx: Optional[int] = None  # 记录转发到的目标槽位，便于终端绘图


class VisualCheneyGC:
    """
    带终端内存可视化渲染的 Cheney 垃圾回收器
    """

    def __init__(self, capacity: int = 8):
        self.capacity = capacity
        self.from_space: List[Optional[Obj]] = [None] * capacity
        self.to_space: List[Optional[Obj]] = [None] * capacity

        self.from_free = 0  # From-space 分配指针
        self.scan = 0       # BFS 队列头 (待扫描字段)
        self.free = 0       # BFS 队列尾 (空闲槽位起始)
        self.roots: List[List[Optional[Obj]]] = []

    def allocate(self, name: str) -> Obj:
        """在当前 From-space 中分配对象（指针碰撞）"""
        if self.from_free >= self.capacity:
            raise MemoryError("From-space 内存已满！")
        obj = Obj(name)
        self.from_space[self.from_free] = obj
        self.from_free += 1
        return obj

    def add_root(self, root_slot: List[Optional[Obj]]):
        """注册根指针引用槽"""
        self.roots.append(root_slot)

    # ==========================================
    # 终端内存 ASCII 渲染器
    # ==========================================
    def draw_heap(self, step_title: str, action_desc: str = ""):
        cell_w = 11  # 单个内存槽位的字符宽度
        line_w = (cell_w + 1) * self.capacity + 1
        divider = "+" + ("-" * cell_w + "+") * self.capacity

        print("\n" + "=" * line_w)
        print(f"  {step_title}")
        if action_desc:
            print(f"  动作说明: {action_desc}")
        print("=" * line_w)

        # 1. 绘制 From-space
        print("\n[ From-space (源空间) ]")
        print(divider)
        # 槽位序号
        row_slots = "|".join(f" Slot {i}".center(cell_w) for i in range(self.capacity))
        print(f"|{row_slots}|")
        print(divider)
        # 对象名称
        row_names = []
        for obj in self.from_space:
            val = obj.name if obj else "."
            row_names.append(val.center(cell_w))
        print("|" + "|".join(row_names) + "|")
        # 转发状态
        row_fwds = []
        for obj in self.from_space:
            if obj and obj.forwarded_idx is not None:
                val = f"fwd->T[{obj.forwarded_idx}]"
            elif obj:
                val = "(alive/garb)"
            else:
                val = ""
            row_fwds.append(val.center(cell_w))
        print("|" + "|".join(row_fwds) + "|")
        print(divider)

        # 2. 绘制 To-space
        print("\n[ To-space (目标空间 / BFS工作队列) ]")
        print(divider)
        print(f"|{row_slots}|")
        print(divider)
        # 对象名称
        row_to_names = []
        for obj in self.to_space:
            val = obj.name if obj else "."
            row_to_names.append(val.center(cell_w))
        print("|" + "|".join(row_to_names) + "|")
        # 对象内部字段所指的目标名称
        row_to_fields = []
        for obj in self.to_space:
            if obj and obj.fields:
                targets = ",".join(f.name if f else "." for f in obj.fields)
                val = f"[{targets}]"
            elif obj:
                val = "[]"
            else:
                val = ""
            row_to_fields.append(val.center(cell_w))
        print("|" + "|".join(row_to_fields) + "|")
        print(divider)

        # 3. 绘制 scan 与 free 指针指示箭头
        arrow_buf = list(" " * line_w)
        label_buf = list(" " * line_w)

        def get_center_col(idx: int) -> int:
            if idx < self.capacity:
                return 1 + idx * (cell_w + 1) + cell_w // 2
            return line_w - 2  # 队列已满指向末尾

        scan_col = get_center_col(self.scan)
        free_col = get_center_col(self.free)

        if self.scan == self.free:
            arrow_buf[scan_col] = "^"
            txt = f"scan,free={self.scan}"
            start = max(0, scan_col - len(txt) // 2)
            label_buf[start : start + len(txt)] = list(txt)
        else:
            arrow_buf[scan_col] = "^"
            txt_scan = f"scan={self.scan}"
            start_s = max(0, scan_col - len(txt_scan) // 2)
            label_buf[start_s : start_s + len(txt_scan)] = list(txt_scan)

            arrow_buf[free_col] = "^"
            txt_free = f"free={self.free}"
            start_f = max(0, free_col - len(txt_free) // 2)
            label_buf[start_f : start_f + len(txt_free)] = list(txt_free)

        print("".join(arrow_buf))
        print("".join(label_buf))

        # 4. 打印队列区段解析
        scanned_part = f"0..{self.scan-1}" if self.scan > 0 else "无"
        queue_part = f"{self.scan}..{self.free-1}" if self.scan < self.free else "空"
        free_part = f"{self.free}..{self.capacity-1}" if self.free < self.capacity else "无"
        print(f"  >> 状态解读: 已扫描区:[{scanned_part}] | BFS队列中:[{queue_part}] | 空闲区:[{free_part}]")
        print("=" * line_w + "\n")

    # ==========================================
    # 垃圾回收核心流程
    # ==========================================
    def copy(self, obj: Optional[Obj]) -> Optional[Obj]:
        if obj is None:
            return None

        # 1. 已转发：直接返回已经拷贝在 To-space 的地址
        if obj.forwarded is not None:
            return obj.forwarded

        # 2. 未转发：拷贝到 To-space[free] 处
        dest_idx = self.free
        new_obj = Obj(obj.name, list(obj.fields))
        self.to_space[dest_idx] = new_obj
        self.free += 1

        # 在原对象处留下转发指针与目标槽位标记
        obj.forwarded = new_obj
        obj.forwarded_idx = dest_idx

        return new_obj

    def collect(self):
        # 初始：初始化 scan 与 free 指针
        self.scan = 0
        self.free = 0

        self.draw_heap(
            step_title="【GC 启动】初始状态",
            action_desc="From-space 包含存活与垃圾对象，To-space 全空，scan=free=0",
        )

        # 阶段 1: 复制根集直接引用的对象
        for i, root_slot in enumerate(self.roots):
            if root_slot[0]:
                old_target = root_slot[0]
                root_slot[0] = self.copy(old_target)
                self.draw_heap(
                    step_title=f"【阶段 1: 根集处理】处理 Root_{i} (指向 {old_target.name})",
                    action_desc=f"将根对象 [{old_target.name}] 拷贝至 To-space[0]，原对象设转发指针，Root 指向新副本",
                )

        # 阶段 2: BFS 扫描工作队列 (scan < free)
        step = 1
        while self.scan < self.free:
            curr = self.to_space[self.scan]
            scan_idx = self.scan

            # 遍历当前扫描对象的所有字段
            for field_idx, child in enumerate(curr.fields):
                if child is None:
                    continue

                if child.forwarded is not None:
                    # 子节点已被复制：重定向指针
                    curr.fields[field_idx] = child.forwarded
                    self.draw_heap(
                        step_title=f"【阶段 2: BFS 扫描第 {step} 轮】处理槽位 [{scan_idx}] 对象 [{curr.name}] 的字段",
                        action_desc=f"子对象 [{child.name}] 已转发至 T[{child.forwarded_idx}]，直接重定向字段指针（打破环/菱形）",
                    )
                else:
                    # 子节点尚未复制：拷贝入队
                    dest = self.free
                    curr.fields[field_idx] = self.copy(child)
                    self.draw_heap(
                        step_title=f"【阶段 2: BFS 扫描第 {step} 轮】处理槽位 [{scan_idx}] 对象 [{curr.name}] 的字段",
                        action_desc=f"发现新存活子对象 [{child.name}]，拷贝至 To-space[{dest}]（入队），推进 free 指针",
                    )

            # 当前对象的所有字段已扫描完毕，推进 scan 指针出队
            self.scan += 1
            step += 1
            self.draw_heap(
                step_title=f"【阶段 2: 节点扫描完成】槽位 [{scan_idx}] 处理完毕",
                action_desc=f"对象 [{curr.name}] 字段均已就绪，推进 scan 指针（出队）至 [{self.scan}]",
            )

        # 阶段 3: 空间对调 (Flip)
        self.from_space = self.to_space
        self.from_free = self.free
        self.to_space = [None] * self.capacity
        self.scan = 0
        self.free = 0

        self.draw_heap(
            step_title="【阶段 3: 空间对调 (Flip)】GC 结束",
            action_desc="交换 From/To-space，原 From 空间全部废弃（垃圾自然回收），所有存活对象已紧凑排列",
        )


# ==========================================
# 场景执行：构造环状、菱形和孤岛垃圾
# ==========================================
if __name__ == "__main__":
    gc = VisualCheneyGC(capacity=8)

    # 1. 存活对象拓扑结构:
    #    Root -> A
    #    A -> [B, C]
    #    B -> [D]
    #    C -> [B]   (菱形引用: A与C均引用B)
    #    D -> [A]   (环状引用: D回指A)
    obj_a = gc.allocate("A")
    obj_b = gc.allocate("B")
    obj_c = gc.allocate("C")
    obj_d = gc.allocate("D")

    obj_a.fields = [obj_b, obj_c]
    obj_b.fields = [obj_d]
    obj_c.fields = [obj_b]
    obj_d.fields = [obj_a]

    # 2. 垃圾对象 (相互引用但根不可达的孤岛):
    garb_x = gc.allocate("Garbage_X")
    garb_y = gc.allocate("Garbage_Y")
    garb_x.fields = [garb_y]
    garb_y.fields = [garb_x]

    # 3. 根节点注册
    root = [obj_a]
    gc.add_root(root)

    # 4. 执行垃圾回收
    gc.collect()

