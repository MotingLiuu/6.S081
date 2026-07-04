# polish

Expressions
1. unit expression `var`, `()`, with highest priority.
2. expression level2 with `*/%`, `unit op unit op ...`, with second priority.
3. expression level3 with `+-`, `exlevel2 op exlevel2 op ...`, with third priority.

## Convert to postfix
Consider expression with level3, `ex<=level2 op ex<=level2 op ...`
1. deal with `ex <= level2` first
2. deal with second `op`, then print `op`.
3. ...continue

Without recursion:

stack vars
stack ops

while:
    if '(', create a new frame (push '(' to stack, treat everything after '(' as a new stack.
    if 'v', just print. it is a ex just contain itself.
    if `op`: there are two cases:
        1. if current frame is empty, just push `op` to stack(create a new frame to compute right ex). 
        2. if current frame is not empty,
            1. if frame top's op's priority is higher than 'op', then pop 'op' from stack(end current frame), and push it to current frame(create a new frame to compute right ex).
            2. if frame top's op's priority is lower than 'op', push 'op' to stack(create a new frmae to compute right ex).
    if ')', pop frames from stack untile find '('.

        
    





















