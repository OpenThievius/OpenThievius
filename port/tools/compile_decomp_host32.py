#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Compile Sly decompilation C++ directly for a native 32-bit host ABI."""

import argparse
import re
import shutil
import shlex
import subprocess
import sys
from pathlib import Path


def lower_named_returns(text, label):
    """Lower GCC 2.95's named-return extension to ordinary host C++."""
    pattern = re.compile(
        r"(?m)^(?P<indent>[ \t]*)(?P<prefix>(?:(?:static|inline)\s+)*"
        r"(?P<type>[A-Za-z_]\w*(?:::\w+)*)\s+[^\n{};]*\))"
        r"\s+return\s+(?P<result>[A-Za-z_]\w*)\s*\n(?P<open>[ \t]*)\{")
    matches = list(pattern.finditer(text))
    for match in reversed(matches):
        opening = match.end() - 1
        depth = 0
        state = "code"
        escaped = False
        closing = None
        for index in range(opening, len(text)):
            char = text[index]
            nxt = text[index + 1] if index + 1 < len(text) else ""
            if state == "line_comment":
                if char == "\n": state = "code"
                continue
            if state == "block_comment":
                if char == "*" and nxt == "/": state = "code"
                continue
            if state in ("string", "char"):
                if escaped: escaped = False
                elif char == "\\": escaped = True
                elif (state == "string" and char == '"') or (state == "char" and char == "'"):
                    state = "code"
                continue
            if char == "/" and nxt == "/": state = "line_comment"; continue
            if char == "/" and nxt == "*": state = "block_comment"; continue
            if char == '"': state = "string"; continue
            if char == "'": state = "char"; continue
            if char == "{": depth += 1
            elif char == "}":
                depth -= 1
                if depth == 0:
                    closing = index
                    break
        if closing is None:
            raise RuntimeError("unclosed named-return body in " + label)
        replacement = (match.group("indent") + match.group("prefix") + "\n" +
                       match.group("open") + "{\n" + match.group("open") + "    " +
                       match.group("type") + " " + match.group("result") + ";")
        text = (text[:match.start()] + replacement + text[match.end():closing] +
                "\n" + match.group("open") + "    return " + match.group("result") + ";" +
                "\n" + text[closing:])
    return text


def escape_literal_newlines(text):
    """Convert raw line breaks inside ordinary string/character literals."""
    out = []
    state = "code"
    escaped = False
    i = 0
    while i < len(text):
        c = text[i]
        n = text[i + 1] if i + 1 < len(text) else ""
        if state == "line_comment":
            out.append(c)
            if c == "\n": state = "code"
        elif state == "block_comment":
            out.append(c)
            if c == "*" and n == "/":
                out.append(n); i += 1; state = "code"
        elif state in ("string", "char"):
            if c == "\n":
                out.append(r"\n")
                escaped = False
            else:
                out.append(c)
                if escaped: escaped = False
                elif c == "\\": escaped = True
                elif (state == "string" and c == '"') or (state == "char" and c == "'"):
                    state = "code"
        else:
            out.append(c)
            if c == "/" and n == "/": out.append(n); i += 1; state = "line_comment"
            elif c == "/" and n == "*": out.append(n); i += 1; state = "block_comment"
            elif c == '"': state = "string"
            elif c == "'": state = "char"
        i += 1
    return "".join(out)


def lower_gcc_minmax(text):
    """Lower the old GCC ``>?`` maximum operator in staged C++ units."""
    compound_used = False
    text, count = re.subn(
        r"\b(?P<lhs>[A-Za-z_]\w*)\s*>\?=\s*(?P<rhs>[A-Za-z_]\w*)",
        lambda m: m.group("lhs") + " = sly1_host_max(" + m.group("lhs") +
        ", " + m.group("rhs") + ")", text)
    compound_used |= count != 0
    text, count = re.subn(
        r"(?P<lhs>\*\(f32\*\)&self->f26A0)\s*>\?=\s*"
        r"(?P<rhs>\*\(f32\*\)\(\(u8\*\)self \+ 0xA(?:40|20)\))",
        lambda m: m.group("lhs") + " = sly1_host_max(" + m.group("lhs") +
        ", " + m.group("rhs") + ")", text)
    compound_used |= count != 0
    changed = False
    while True:
        points = []
        state = "code"
        escaped = False
        i = 0
        while i < len(text):
            c = text[i]
            n = text[i + 1] if i + 1 < len(text) else ""
            if state == "line":
                if c == "\n": state = "code"
                i += 1; continue
            if state == "comment":
                if c == "*" and n == "/": state = "code"; i += 2; continue
                i += 1; continue
            if state in ("string", "char"):
                if escaped: escaped = False
                elif c == "\\": escaped = True
                elif (state == "string" and c == '"') or (state == "char" and c == "'"): state = "code"
                i += 1; continue
            if c == "/" and n == "/": state = "line"; i += 2; continue
            if c == "/" and n == "*": state = "comment"; i += 2; continue
            if c == '"': state = "string"; i += 1; continue
            if c == "'": state = "char"; i += 1; continue
            if c in "<>" and n == "?": points.append((i, c)); i += 2; continue
            i += 1
        if not points:
            break

        # Lower one operator at a time, from the right. Re-scanning after
        # each replacement keeps nested min/max expressions' offsets valid.
        op, operator = points[-1]
        left = op - 1
        while left >= 0 and text[left].isspace(): left -= 1
        paren = bracket = 0
        while left >= 0:
            c = text[left]
            if c == ")": paren += 1
            elif c == "(":
                if paren == 0: break
                paren -= 1
            elif c == "]": bracket += 1
            elif c == "[":
                if bracket == 0: break
                bracket -= 1
            if (paren == 0 and bracket == 0 and
                    (c in "=,;{}?" or (c == ":" and
                     (left == 0 or text[left - 1] != ":") and
                     (left + 1 >= len(text) or text[left + 1] != ":")))): break
            left -= 1
        left += 1
        while left < op and text[left].isspace(): left += 1
        prefix = ""
        replace_left = left
        if text.startswith("return", left) and (left + 6 == op or text[left + 6].isspace()):
            prefix = "return "
            replace_left = left
            left += 6
            while left < op and text[left].isspace(): left += 1

        right = op + 2
        while right < len(text) and text[right].isspace(): right += 1
        end = right
        paren = bracket = 0
        while end < len(text):
            c = text[end]
            if c == "(": paren += 1
            elif c == ")":
                if paren == 0: break
                paren -= 1
            elif c == "[": bracket += 1
            elif c == "]":
                if bracket == 0: break
                bracket -= 1
            if (paren == 0 and bracket == 0 and
                    (c in ",;{}?" or (c == ":" and
                     (end == 0 or text[end - 1] != ":") and
                     (end + 1 >= len(text) or text[end + 1] != ":")))): break
            end += 1
        while end > right and text[end - 1].isspace(): end -= 1
        if left == op or right == end:
            break
        lhs = text[left:op].strip()
        rhs = text[right:end].strip()
        helper_name = "sly1_host_max" if operator == ">" else "sly1_host_min"
        replacement = prefix + helper_name + "((" + lhs + "), (" + rhs + "))"
        text = text[:replace_left] + replacement + text[end:]
        changed = True
    if changed or compound_used:
        helper = ("template<class A, class B> static inline auto sly1_host_max(A a, B b) "
                  "-> decltype(a + b) { return a > b ? a : b; }\n"
                  "template<class A, class B> static inline auto sly1_host_min(A a, B b) "
                  "-> decltype(a + b) { return a < b ? a : b; }\n")
        text = helper + text
    return text


def lower_host_vu0(text):
    """Replace VU0-only inline assembly with equivalent host float-vector math."""
    helpers = r"""
    union HostLanes { u128 q; f32 f[4]; };
    static f32 HostLane(u128 q, int i) { HostLanes v; v.q = q; return v.f[i]; }
    static u128 HostPack(f32 x, f32 y, f32 z, f32 w) {
        HostLanes v{}; v.f[0] = x; v.f[1] = y; v.f[2] = z; v.f[3] = w; return v.q;
    }
    static u32 HostBits(f32 f) { union { f32 f; u32 u; } v; v.f = f; return v.u; }
    static f32 HostFloat(u32 u) { union { f32 f; u32 u; } v; v.u = u; return v.f; }
    static u128 HostWeighted3(u128 r0, u128 r1, u128 r2, u128 v) {
        f32 x = HostLane(v, 0), y = HostLane(v, 1), z = HostLane(v, 2);
        return HostPack(HostLane(r0,0)*x + HostLane(r1,0)*y + HostLane(r2,0)*z,
                        HostLane(r0,1)*x + HostLane(r1,1)*y + HostLane(r2,1)*z,
                        HostLane(r0,2)*x + HostLane(r1,2)*y + HostLane(r2,2)*z,
                        HostLane(v,3));
    }
    static u128 HostWeighted4(u128 r0, u128 r1, u128 r2, u128 r3, u128 v) {
        return HostPack(HostLane(r0,0)*HostLane(v,0)+HostLane(r1,0)*HostLane(v,1)+HostLane(r2,0)*HostLane(v,2)+HostLane(r3,0)*HostLane(v,3),
                        HostLane(r0,1)*HostLane(v,0)+HostLane(r1,1)*HostLane(v,1)+HostLane(r2,1)*HostLane(v,2)+HostLane(r3,1)*HostLane(v,3),
                        HostLane(r0,2)*HostLane(v,0)+HostLane(r1,2)*HostLane(v,1)+HostLane(r2,2)*HostLane(v,2)+HostLane(r3,2)*HostLane(v,3),
                        HostLane(r0,3)*HostLane(v,0)+HostLane(r1,3)*HostLane(v,1)+HostLane(r2,3)*HostLane(v,2)+HostLane(r3,3)*HostLane(v,3));
    }
    """
    marker = "struct Vu0 {"
    if marker not in text:
        return text
    text = text.replace(marker, marker + helpers, 1)
    bodies = {
        "Zero": "return HostPack(0,0,0,0);",
        "Ones": "return HostPack(1,1,1,1);",
        "Copy": "return a;",
        "Add": "return HostPack(HostLane(a,0)+HostLane(b,0),HostLane(a,1)+HostLane(b,1),HostLane(a,2)+HostLane(b,2),HostLane(a,3)+HostLane(b,3));",
        "Sub": "return HostPack(HostLane(a,0)-HostLane(b,0),HostLane(a,1)-HostLane(b,1),HostLane(a,2)-HostLane(b,2),HostLane(a,3)-HostLane(b,3));",
        "FromFloat": "return HostPack(f,0,0,0);",
        "FromFloatAt": "return HostPack(f,0,0,0);",
        "Scalar": "return HostPack(f,0,0,0);",
        "ScalarAt": "return HostPack(f,0,0,0);",
        "ScalarAtValue": "return HostPack(f,0,0,0);",
        "FromFloatAtCall": "return HostPack(f,0,0,0);",
        "ScalarAtCall": "return HostPack(f,0,0,0);",
        "ToFloat": "return HostLane(a,0);",
        "ToFloatMoved": "return HostLane(a,0);",
        "ToFloatNow": "return HostLane(a,0);",
        "ToFloatViaBits": "return HostLane(a,0);",
        "ToBits": "return HostBits(HostLane(a,0));",
        "ToFloatY": "return HostLane(a,1);",
        "ToFloatZ": "return HostLane(a,2);",
        "StoreX": "*(u32*)&dst = HostBits(HostLane(a,0));",
        "Splat": "return HostPack(f,f,f,f);",
        "FromFloats": "return HostPack(x,y,z,z);",
        "ScalarInGpr": "return HostPack(f,0,0,0);",
        # The Wave 39 script machine loads a float's bits straight from memory
        # into a GPR before treating it as a scalar VU value.
        "ScalarInGprAt": "return HostPack(f,0,0,0);",
        "ScalarInGprReg": "return HostPack(f,0,0,0);",
        "ScalarMovedReg": "return HostPack(f,0,0,0);",
        # These helpers drive EE VU0 registers and microprograms. Keep the
        # direct-host copy buildable; the currently supported native path has
        # no VU0 execution service, so light collection reports an empty mask.
        "LoadVf": "(void)p;",
        "CallMs": "",
        "MoveToVi": "(void)v;",
        "ReadViWait": "return 0;",
        "NextLightGroup": "(void)firstSlot; (void)count; return 0;",
        "MulX": "f32 scale=HostLane(s,0); return HostPack(HostLane(a,0)*scale,HostLane(a,1)*scale,HostLane(a,2)*scale,HostLane(a,3)*scale);",
        "MulAddX": "f32 scale=HostLane(s,0); return HostPack(HostLane(a,0)+HostLane(b,0)*scale,HostLane(a,1)+HostLane(b,1)*scale,HostLane(a,2)+HostLane(b,2)*scale,HostLane(a,3)+HostLane(b,3)*scale);",
        "MulAddX2": "f32 sx=HostLane(s,0),tx=HostLane(t,0); return HostPack(HostLane(a,0)+HostLane(b,0)*sx+HostLane(c,0)*tx,HostLane(a,1)+HostLane(b,1)*sx+HostLane(c,1)*tx,HostLane(a,2)+HostLane(b,2)*sx+HostLane(c,2)*tx,HostLane(a,3)+HostLane(b,3)*sx+HostLane(c,3)*tx);",
        "MulAddX2WeightFirst": "f32 sx=HostLane(s,0),tx=HostLane(t,0); return HostPack(HostLane(a,0)+HostLane(b,0)*sx+HostLane(c,0)*tx,HostLane(a,1)+HostLane(b,1)*sx+HostLane(c,1)*tx,HostLane(a,2)+HostLane(b,2)*sx+HostLane(c,2)*tx,HostLane(a,3)+HostLane(b,3)*sx+HostLane(c,3)*tx);",
        "MulAddX3": "f32 sx=HostLane(s,0),tx=HostLane(t,0),ux=HostLane(u,0); return HostPack(HostLane(a,0)+HostLane(b,0)*sx+HostLane(c,0)*tx+HostLane(d,0)*ux,HostLane(a,1)+HostLane(b,1)*sx+HostLane(c,1)*tx+HostLane(d,1)*ux,HostLane(a,2)+HostLane(b,2)*sx+HostLane(c,2)*tx+HostLane(d,2)*ux,HostLane(a,3)+HostLane(b,3)*sx+HostLane(c,3)*tx+HostLane(d,3)*ux);",
        "Dot": "return HostPack(HostLane(a,0)*HostLane(b,0)+HostLane(a,1)*HostLane(b,1)+HostLane(a,2)*HostLane(b,2),HostLane(a,1)*HostLane(b,1),HostLane(a,2)*HostLane(b,2),0);",
        "SelfDot": "return HostPack(HostLane(v,0)*HostLane(v,0)+HostLane(v,1)*HostLane(v,1)+HostLane(v,2)*HostLane(v,2),HostLane(v,1)*HostLane(v,1),HostLane(v,2)*HostLane(v,2),0);",
        "DotXY": "return HostPack(HostLane(a,0)*HostLane(b,0)+HostLane(a,1)*HostLane(b,1),HostLane(a,1)*HostLane(b,1),0,0);",
        "Sqrt": "return HostPack(__builtin_sqrtf(HostLane(a,0)),HostLane(a,1),HostLane(a,2),HostLane(a,3));",
        "DivX": "f32 q=1.0f/HostLane(s,0); return HostPack(HostLane(a,0)*q,HostLane(a,1)*q,HostLane(a,2)*q,HostLane(a,3)*q);",
        "Cross": "return HostPack(HostLane(a,1)*HostLane(b,2)-HostLane(a,2)*HostLane(b,1),HostLane(a,2)*HostLane(b,0)-HostLane(a,0)*HostLane(b,2),HostLane(a,0)*HostLane(b,1)-HostLane(a,1)*HostLane(b,0),0);",
        "Min": "return HostPack(__builtin_fminf(HostLane(a,0),HostLane(b,0)),__builtin_fminf(HostLane(a,1),HostLane(b,1)),__builtin_fminf(HostLane(a,2),HostLane(b,2)),HostLane(a,3));",
        "Max": "return HostPack(__builtin_fmaxf(HostLane(a,0),HostLane(b,0)),__builtin_fmaxf(HostLane(a,1),HostLane(b,1)),__builtin_fmaxf(HostLane(a,2),HostLane(b,2)),HostLane(a,3));",
        "Rotate": "const u128* r=(const u128*)&m; return HostWeighted3(r[0],r[1],r[2],v);",
        "RotateRows": "return HostWeighted3(r0,r1,r2,v);",
        "Transform": "const u128* r=(const u128*)&m; u128 v4=HostPack(HostLane(v,0),HostLane(v,1),HostLane(v,2),w); return HostWeighted4(r[0],r[1],r[2],r[3],v4);",
        "TransformRows": "u128 v4=HostPack(HostLane(v,0),HostLane(v,1),HostLane(v,2),w); return HostWeighted4(r0,r1,r2,r3,v4);",
        "Transform4": "return HostWeighted4(m0,m1,m2,m3,v);",
        "RotateTransposed": "const u128* r=(const u128*)&m; return HostPack(HostLane(r[0],0)*HostLane(v,0)+HostLane(r[0],1)*HostLane(v,1)+HostLane(r[0],2)*HostLane(v,2),HostLane(r[1],0)*HostLane(v,0)+HostLane(r[1],1)*HostLane(v,1)+HostLane(r[1],2)*HostLane(v,2),HostLane(r[2],0)*HostLane(v,0)+HostLane(r[2],1)*HostLane(v,1)+HostLane(r[2],2)*HostLane(v,2),HostLane(v,3));",
        "RotateTransposedMembers": "const Vec* r=(const Vec*)&m; return HostPack(HostLane(r[0].q,0)*HostLane(v,0)+HostLane(r[0].q,1)*HostLane(v,1)+HostLane(r[0].q,2)*HostLane(v,2),HostLane(r[1].q,0)*HostLane(v,0)+HostLane(r[1].q,1)*HostLane(v,1)+HostLane(r[1].q,2)*HostLane(v,2),HostLane(r[2].q,0)*HostLane(v,0)+HostLane(r[2].q,1)*HostLane(v,1)+HostLane(r[2].q,2)*HostLane(v,2),HostLane(v,3));",
        "MulXSum": "f32 sx=HostLane(s,0),tx=HostLane(t,0); return HostPack(HostLane(a,0)*sx+HostLane(b,0)*tx,HostLane(a,1)*sx+HostLane(b,1)*tx,HostLane(a,2)*sx+HostLane(b,2)*tx,HostLane(a,3)*sx+HostLane(b,3)*tx);",
        "PlaneSigns": "u128 d=HostPack(HostLane(r0,0)*HostLane(v,0)+HostLane(r1,0)*HostLane(v,1)+HostLane(r2,0)*HostLane(v,2),HostLane(r0,1)*HostLane(v,0)+HostLane(r1,1)*HostLane(v,1)+HostLane(r2,1)*HostLane(v,2),HostLane(r0,2)*HostLane(v,0)+HostLane(r1,2)*HostLane(v,1)+HostLane(r2,2)*HostLane(v,2),HostLane(r0,3)*HostLane(v,0)+HostLane(r1,3)*HostLane(v,1)+HostLane(r2,3)*HostLane(v,2)); u32 f=0; for(int i=0;i<4;i++) if(HostLane(d,i)-HostLane(ones,i)*HostLane(s,0)<0) f|=(0x80u>>i); return f;",
        "MulXSum3": "f32 sx=HostLane(s,0),tx=HostLane(t,0),ux=HostLane(u,0); return HostPack(HostLane(a,0)*sx+HostLane(b,0)*tx+HostLane(c,0)*ux,HostLane(a,1)*sx+HostLane(b,1)*tx+HostLane(c,1)*ux,HostLane(a,2)*sx+HostLane(b,2)*tx+HostLane(c,2)*ux,HostLane(a,3)*sx+HostLane(b,3)*tx+HostLane(c,3)*ux);",
        "MulXSum4": "f32 sx=HostLane(s,0),tx=HostLane(t,0),ux=HostLane(u,0),vx=HostLane(v,0); return HostPack(HostLane(a,0)*sx+HostLane(b,0)*tx+HostLane(c,0)*ux+HostLane(d,0)*vx,HostLane(a,1)*sx+HostLane(b,1)*tx+HostLane(c,1)*ux+HostLane(d,1)*vx,HostLane(a,2)*sx+HostLane(b,2)*tx+HostLane(c,2)*ux+HostLane(d,2)*vx,HostLane(a,3)*sx+HostLane(b,3)*tx+HostLane(c,3)*ux+HostLane(d,3)*vx);",
        "RotateUntied": "const u128* r=(const u128*)&m; return HostWeighted3(r[0],r[1],r[2],v);",
        "RotateMembers": "const u128* r=(const u128*)&m; return HostWeighted3(r[0],r[1],r[2],v);",
        "RotateRowsTied": "return HostWeighted3(r0,r1,r2,v);",
        "Transpose": "Rows r; for(int i=0;i<3;i++) r.r[i]=HostPack(HostLane(m.r[0],i),HostLane(m.r[1],i),HostLane(m.r[2],i),0); return r;",
        "MulTransposed": "const u128* p=(const u128*)&a; const u128* q=(const u128*)&b; u128* r=(u128*)&out; for(int j=0;j<3;j++) r[j]=HostPack(HostLane(q[0],0)*HostLane(p[0],j)+HostLane(q[1],0)*HostLane(p[1],j)+HostLane(q[2],0)*HostLane(p[2],j),HostLane(q[0],1)*HostLane(p[0],j)+HostLane(q[1],1)*HostLane(p[1],j)+HostLane(q[2],1)*HostLane(p[2],j),HostLane(q[0],2)*HostLane(p[0],j)+HostLane(q[1],2)*HostLane(p[1],j)+HostLane(q[2],2)*HostLane(p[2],j),HostLane(q[0],3)*HostLane(p[0],j)+HostLane(q[1],3)*HostLane(p[1],j)+HostLane(q[2],3)*HostLane(p[2],j));",
    }

    def matching_brace(source, opening):
        depth, state, escaped = 0, "code", False
        for i in range(opening, len(source)):
            c = source[i]
            n = source[i + 1] if i + 1 < len(source) else ""
            if state == "line":
                if c == "\n": state = "code"
                continue
            if state == "comment":
                if c == "*" and n == "/": state = "code"
                continue
            if state in ("string", "char"):
                if escaped: escaped = False
                elif c == "\\": escaped = True
                elif (state == "string" and c == '"') or (state == "char" and c == "'"): state = "code"
                continue
            if c == "/" and n == "/": state = "line"; continue
            if c == "/" and n == "*": state = "comment"; continue
            if c == '"': state = "string"; continue
            if c == "'": state = "char"; continue
            if c == "{": depth += 1
            elif c == "}":
                depth -= 1
                if depth == 0: return i
        raise RuntimeError("unclosed VU0 method while staging")

    # The particle filter is an EE VU0 microprogram, not host inline
    # assembly. Leave its CPU-visible particle records intact in the native
    # staging copy until a dedicated host implementation is added.
    particle_marker = "struct ParticleFilter {"
    if particle_marker in text:
        start = text.index(particle_marker)
        opening = text.index("{", start)
        closing = matching_brace(text, opening)
        replacement = """struct ParticleFilter {
        static void Start(u128, u128, u128, u128, u128, u128, u128, u128, u128) {}
        static void Next(u128, u128) {}
        static void Wait() {}
        static void Results(u128&, u128&) {}
        static void Bounds(u128&, u128&) {}
    };"""
        text = text[:start] + replacement + text[closing + 1:]

    for name, body in bodies.items():
        pattern = re.compile(
            r"(?m)^[ \t]*(?:template\s*<[^\n]*>\s*)?(?:static\s+)?[^\n;{}]*\b" +
            re.escape(name) + r"\s*\([^\n;{}]*\)\s*\n[ \t]*\{")
        cursor = 0
        while True:
            match = pattern.search(text, cursor)
            if match is None: break
            opening = text.find("{", match.start(), match.end())
            closing = matching_brace(text, opening)
            if "__asm__" in text[opening:closing]:
                replacement = body
                signature = text[match.start():opening]
                if name == "StoreX" and re.search(r"\(T& dst,\s*u32 bits\)", signature):
                    replacement = "*(u32*)&dst = bits;"
                text = text[:opening + 1] + "\n        " + replacement + "\n    " + text[closing:]
                cursor = opening + len(replacement) + 2
            else:
                cursor = closing + 1
    return text


def lower_host_system(text):
    """Give the few EE COP0/interrupt helpers host-safe compile-time bodies."""
    replacements = {
        '__asm__ volatile("sync");': "__sync_synchronize();",
        '__asm__ volatile("sync\\n\\tei");': "__sync_synchronize();",
        '__asm__ volatile("mfc0 %0, $12" : "=r"(r));': "r = 0;",
        '__asm__ volatile("mfc0 %0, $9" : "=r"(r));': "r = 0;",
        '__asm__ volatile("mtc0 %z0, $6\\n\\tsync.p" : : "dJ"(n));': "(void)n;",
    }
    for source, target in replacements.items():
        text = text.replace(source, target)
    text = re.sub(
        r"static __inline__ s32 CurrentThreadIdFromInterrupt\(void\)\s*\{.*?\n\}",
        "static __inline__ s32 CurrentThreadIdFromInterrupt(void)\n{\n    return -1;\n}",
        text, flags=re.S)
    return text


def align_host_quad_arrays(text):
    """Retain the EE alignment of four-word globals in the i386 staging copy.

    The matching source often declares a VU quadword as a plain scalar array
    (for example ``f32 value[4]``), since its retail address provides the
    required alignment. The host linker lays these symbols out independently
    and otherwise gives them only scalar alignment. Native SIMD loads then
    fault even though the source data is valid. Over-aligning these host-only
    arrays preserves their four-word contents and leaves the decomp untouched.
    """
    pattern = re.compile(
        r"(?m)^(?P<prefix>[ \t]*(?:(?:static|const|volatile|extern)\s+)*)"
        r"(?P<type>(?:s32|u32|f32))\s+"
        r"(?P<name>[A-Za-z_]\w*)\s*(?P<array>\[\s*4\s*\])"
        r"(?P<tail>\s*(?:=|;))")

    def replace(match):
        return (match.group("prefix") + match.group("type") +
                " __attribute__((aligned(16))) " + match.group("name") +
                match.group("array") + match.group("tail"))

    return pattern.sub(replace, text)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", required=True, type=Path, help="read-only sly1-decomp checkout")
    parser.add_argument("--out", required=True, type=Path, help="staged source and object output directory")
    parser.add_argument("--port", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--cxx", default="c++")
    parser.add_argument("--trace-functions", action="store_true",
                        help="add targeted startup state traces without instrumenting game calls")
    args = parser.parse_args(argv)

    source = args.source.expanduser().resolve()
    out = args.out.expanduser().resolve()
    port = args.port.resolve()
    include = source / "include"
    names_header = include / "names.h"
    # This launcher targets NTSC-U. A proven whole-file unit replaces only its
    # own address range, not an entire 0x4000 window that happens to overlap it.
    # Keep chunks with uncovered text so matching functions after a file's end
    # are not silently dropped from the native build.
    unsupported_native_units = {
        "src/lib/throw.c",           # PS2 __builtin_eh_return / unwind ABI
        "src/lib/exception.cpp",     # requires the PS2 C++ exception runtime
        "src/lib/type_info_kinds.cpp", # depends on host RTTI runtime symbols
    }
    whole_files = []
    whole_ranges = []
    files_table = source / "config" / "us" / "files.tsv"
    if files_table.is_file():
        for row in files_table.read_text(encoding="utf-8").splitlines()[1:]:
            fields = row.split("\t")
            if len(fields) < 3:
                continue
            unit = source / fields[0]
            if unit.is_file() and fields[0] not in unsupported_native_units:
                whole_files.append(unit)
                whole_ranges.append((int(fields[1], 16), int(fields[2], 16)))
    main = source / "src" / "main"
    chunks = []
    if main.is_dir():
        for path in main.glob("[0-9A-Fa-f]*.cpp"):
            try:
                address = int(path.stem, 16)
            except ValueError:
                continue
            window_end = address + 0x4000
            fully_covered = any(start <= address and window_end <= end
                                for start, end in whole_ranges)
            if not fully_covered:
                chunks.append(path)
    sources = sorted(set(whole_files + chunks + [
        path
        for directory in ("lib", "libm")
        for path in (source / "src" / directory).rglob("*.cpp")
        if path.relative_to(source).as_posix() not in unsupported_native_units
    ]))
    if not sources:
        parser.error("no C++ source units found under " + str(source / "src"))
    compiler = shlex.split(args.cxx)
    failures = []
    warnings = 0
    staged_root = out / "staged"
    objects_root = out / "objects"
    staged_include = staged_root / "include"
    shutil.rmtree(staged_root, ignore_errors=True)
    shutil.rmtree(objects_root, ignore_errors=True)
    staged_root.mkdir(parents=True, exist_ok=True)
    objects_root.mkdir(parents=True, exist_ok=True)
    staged_include.mkdir(parents=True, exist_ok=True)
    # Quoted includes inside decomp headers search their own directory before
    # any -I path. Stage the full header tree so nested includes cannot bypass
    # host-only adapters (notably vu0.h) and read the unmodified EE assembly.
    for header in include.rglob("*"):
        if not header.is_file():
            continue
        staged_header = staged_include / header.relative_to(include)
        staged_header.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(header, staged_header)
    wide_header = staged_include / "mul_wide.h"
    if wide_header.is_file():
        # The upstream macro emits EE multu/HI/LO constraints. The native
        # build needs the same unsigned 32x32 -> 64-bit result on the host.
        wide_header.write_text(
            wide_header.read_text(encoding="utf-8").replace(
                '__asm__("multu %2,%3" : "=l"(low), "=h"(high) : "d"(a), "d"(b))',
                "do { u64 sly1_product = (u64)(u32)(a) * (u32)(b); "
                "(low) = (u32)sly1_product; (high) = (u32)(sly1_product >> 32); } while (0)"),
            encoding="utf-8")
    rotation_header = staged_include / "mul_rotation.h"
    if rotation_header.is_file():
        # EE GCC's signed/unsigned vector conversions are implicit. Host GCC
        # requires SetRows to use the bit-identical unsigned vector type.
        rotation_header.write_text(
            rotation_header.read_text(encoding="utf-8").replace(
                "const s128& x, const s128& y, const s128& z",
                "const u128& x, const u128& y, const u128& z"),
            encoding="utf-8")
    system_header = staged_include / "system.h"
    if system_header.is_file():
        system_header.write_text(
            lower_host_system(system_header.read_text(encoding="utf-8")),
            encoding="utf-8")
    vu0_header = include / "vu0.h"
    if vu0_header.is_file():
        host_vu0 = lower_host_vu0(lower_named_returns(
            vu0_header.read_text(encoding="utf-8"), "include/vu0.h"))
        # Recent matching helpers use GCC 2.95's cast-lvalue extension to
        # influence EE register allocation. It has the same vector value as
        # `r`, but modern host GCC rejects taking the address of the cast.
        # The EE-only allocation detail is irrelevant to the host ABI.
        host_vu0 = host_vu0.replace(
            "return *(u128*)&(s128)r;", "return r;")
        (staged_include / "vu0.h").write_text(host_vu0, encoding="utf-8")
    backfill_root = port / "game" / "sly1-us" / "native"
    # Native backfills take precedence at their retail address. The current
    # decomp groups functions into named whole-file units, so the old
    # address-named staging rules cannot reliably find every matching body.
    # Rename only matching definitions in the staged copy; calls still use
    # the public names and therefore resolve to the port boundary.
    backfill_addresses = set()
    if backfill_root.exists():
        for path in backfill_root.rglob("*.cpp"):
            if re.fullmatch(r"func_[0-9A-Fa-f]{8}", path.stem):
                backfill_addresses.add(path.stem.removeprefix("func_").lower())
            content = path.read_text(encoding="utf-8")
            # Some port boundaries share one source file. Count only actual
            # address-named definitions there, never addresses merely called
            # by a backfill (those vanilla definitions still belong to Sly).
            for address in re.findall(
                    r"(?m)^\s*(?:extern\s+\"C\"\s+)?"
                    r"(?:[A-Za-z_]\w*(?:::\w+)?\s+)+"
                    r"func_([0-9A-Fa-f]{8})\s*\([^;{}]*\)\s*\{", content):
                backfill_addresses.add(address.lower())
    address_names = {
        address.lower(): name
        for name, address in re.findall(
            r"(?m)^#define\s+(\w+)\s+func_([0-9A-Fa-f]{8})\s*$",
            names_header.read_text(encoding="utf-8") if names_header.is_file() else "")
    }
    host_include = port / "sly1" / "host32" / "include"
    flags = [
        "-m32", "-malign-double", "-msse2", "-mstackrealign", "-std=gnu++20",
        "-fno-exceptions", "-fno-rtti",
        "-O2",
        "-fno-pie", "-fno-pic", "-ffunction-sections", "-fdata-sections",
        "-fno-asynchronous-unwind-tables", "-fno-unwind-tables", "-fpermissive",
        "-include", str(port / "sly1" / "host_abi.h"),
        "-I", str(host_include),
        "-I", str(staged_include),
        "-I", str(include),
    ]
    # host_abi.h defines the retail common.h guard because it supplies the
    # host-width integer aliases. Newer decomp snapshots include generated
    # readable symbol macros from common.h, so force that independent header
    # after the ABI header rather than letting the guard suppress it.
    if names_header.is_file():
        flags[flags.index("-I"):flags.index("-I")] = ["-include", str(names_header)]
    out.mkdir(parents=True, exist_ok=True)

    for src in sources:
        rel = src.relative_to(source / "src")
        staged = staged_root / rel
        obj = objects_root / rel.with_suffix(".o")
        staged.parent.mkdir(parents=True, exist_ok=True)
        obj.parent.mkdir(parents=True, exist_ok=True)
        original = src.read_text(encoding="utf-8")
        # The PS2 compiler treats unsigned long literals as 64-bit; i386 does not.
        adapted = re.sub(r"(?i)(0x[0-9a-f]+|[0-9]+)UL\b", r"\1ULL", original)
        # These two source strings contain literal newlines, which modern C++
        # rejects. Preserve the intended newline characters in the staged copy.
        adapted = adapted.replace(
            '"Sly Cooper\nand the Thievius Raccoonus"',
            '"Sly Cooper\\nand the Thievius Raccoonus"')
        adapted = adapted.replace('"%s\n%s"', '"%s\\n%s"')
        adapted = escape_literal_newlines(adapted)
        adapted = lower_gcc_minmax(lower_named_returns(adapted, rel.as_posix()))
        # The retail ABI uses 64-bit s64/u64 even where the decomp unit spells
        # those aliases with `long`; on host i386, long is only 32 bits.
        adapted = re.sub(
            r"(?m)^\s*typedef\s+(?:signed|unsigned)\s+long\s+[su]64\s*;\s*$",
            "", adapted)
        adapted = align_host_quad_arrays(adapted)
        if rel.as_posix() == "lib/graph.c":
            # This upstream C file relies on old C's implicit function
            # declarations and GNU vector scalar assignment. Keep its logic,
            # adding explicit prototypes and zeroing the two VIF tags by bytes
            # for the modern C++ host compiler.
            adapted = adapted.replace(
                '#include "common.h"',
                '#include "common.h"\n'
                'void WriteDisplayRegisters(void*);\n'
                's32 SendDrawEnvironment(void*);', 1)
            adapted = adapted.replace(
                '*(V128*)&self->tag50 = 0;',
                '__builtin_memset(&self->tag50, 0, sizeof(V128));')
            adapted = adapted.replace(
                '*(V128*)&self->tag140 = 0;',
                '__builtin_memset(&self->tag140, 0, sizeof(V128));')
        elif rel.as_posix() == "lib/allocate.cpp":
            # Dynamic exception specifications were removed after the PS2
            # compiler; they do not affect this native C++ allocation body.
            adapted = adapted.replace(" throw(bad_alloc)", "")
        elif rel.as_posix() == "main/pad_input.cpp":
            adapted = adapted.replace("fabsf(", "__builtin_fabsf(")
        elif rel.as_posix() in ("main/file_1B7968.cpp", "main/collider.cpp"):
            # This helper manually walks the original EE 8-byte vararg slots.
            # The host32 C++ ABI places each word argument in a 4-byte stack
            # slot, so lower just this staged helper to the host layout.
            adapted = adapted.replace(
                "#define VA_ARG_WORD(ap, T) (*(T*)((ap += 8) - 8))",
                "#define VA_ARG_WORD(ap, T) (*(T*)((ap += 4) - 4))")
            adapted = adapted.replace(
                "(char*)__builtin_next_arg(size) - (8 - __builtin_args_info(0)) * 8",
                "(char*)__builtin_next_arg(size)")
        elif rel.as_posix() in ("main/file_1C1190.cpp", "main/object_properties.cpp"):
            adapted = adapted.replace(
                "#define VA_ARG_WORD(ap, T) (*(T*)((ap += 8) - 8))",
                "#define VA_ARG_WORD(ap, T) (*(T*)((ap += 4) - 4))")
            adapted = adapted.replace(
                "(char*)__builtin_next_arg(size) - (8 - __builtin_args_info(0)) * 8",
                "(char*)__builtin_next_arg(size)")
        elif rel.as_posix() in ("main/file_1E88D0.cpp", "main/warp_menu.cpp"):
            # AreFloatsNearlyEqual has a separate native replacement; retain
            # its public address prototype while renaming the upstream body.
            adapted = adapted.replace(
                '#include "common.h"',
                '#include "common.h"\n'
                'extern "C" s32 func_001EAC68(f32, f32, f32);', 1)
            adapted = adapted.replace(
                "void ComputeSinCos(f32* a, f32* b);",
                "void ComputeSinCos(f32 angle, f32* a, f32* b);")
            adapted = adapted.replace(
                "void ComputeSinCos(f32* sinOut, f32* cosOut, f32 angle)",
                "void ComputeSinCos(f32 angle, f32* sinOut, f32* cosOut)")
            adapted = adapted.replace(
                "ComputeSinCos(&p.sine, &p.cosine);",
                "ComputeSinCos(unused, &p.sine, &p.cosine);")
            # ReserveVifUnpack calls WriteVifUnpackHeader with only its first
            # EE register argument. On EE, a1-a3 still hold ReserveVifUnpack's
            # unused1/size/unused3 parameters; the i386 ABI has no such register
            # residue, so make that already-existing argument flow explicit in
            # the host staging copy.
            adapted, replacements = re.subn(
                r"(?m)^        size = WriteVifUnpackHeader\(s\);$",
                "        size = WriteVifUnpackHeader(s, (s32)unused1, size, "
                "(s32)unused3);",
                adapted)
            if replacements != 1:
                raise RuntimeError("expected one EE register-residue call in " +
                                   rel.as_posix())
        elif rel.as_posix() == "main/mode_walker.cpp":
            adapted, replacements = re.subn(
                r"SetMatrixRows\(rot, x, y, z\);",
                "SetMatrixRows(rot, *(const s128*)&x, *(const s128*)&y, "
                "*(const s128*)&z);",
                adapted)
            if replacements != 1:
                raise RuntimeError("expected one host vector signedness conversion")
        elif rel.as_posix() == "main/anim_record.cpp":
            # ConvertHsvToRgb keeps its second output pointer in EE a1 while
            # the C call supplies only the source pointer. Preserve that
            # register flow explicitly for the host stack ABI.
            adapted, replacements = re.subn(
                r"ConvertHsvToRgb\(&tmp\);",
                "ConvertHsvToRgb(&tmp, (Quad_001B4580*)dst);",
                adapted)
            if replacements != 1:
                raise RuntimeError("expected one HSV output register call in " +
                                   rel.as_posix())
        elif rel.as_posix() == "main/iop_commands.cpp":
            # The matching body queues a command but is declared s32 and has
            # no return statement. At -O2 GCC may omit its epilogue entirely,
            # falling into the next function. Callers use it for side effects;
            # return zero in this host staging copy to preserve control flow.
            old = ("s32 SetIopSoundOptionFlag(s32 arg)\n{\n"
                   "    s32 local = arg;\n"
                   "    QueueIopCommand(11, 4, &local, 0, 0);\n"
                   "}")
            new = old[:-2] + "\n    return 0;\n}"
            if adapted.count(old) != 1:
                raise RuntimeError("expected one side-effect-only IOP option helper")
            adapted = adapted.replace(old, new, 1)
        elif rel.as_posix() == "main/pose_source.cpp":
            # These closure byte arrays hold Mat02 values passed to VU0
            # routines. Their retail stack layout is quadword-aligned even
            # though the byte-array declarations do not express it to GCC.
            # Keep their field offsets and alignment in the host frame.
            for closure in ("AxisCurveClosure", "KeyedCurveClosure"):
                old = (f"struct {closure} {{\n    u8 a[0x30];\n"
                       "    u8 b[0x30];\n    u8 c[0x30];")
                new = (f"struct {closure} {{\n"
                       "    u8 a[0x30] __attribute__((aligned(16)));\n"
                       "    u8 b[0x30] __attribute__((aligned(16)));\n"
                       "    u8 c[0x30] __attribute__((aligned(16))); ")
                if adapted.count(old) != 1:
                    raise RuntimeError("expected one matrix closure: " + closure)
                adapted = adapted.replace(old, new, 1)
            # The three concrete weight-curve methods are installed in a
            # WeightCurveEval table, whose host-call signature is
            # (curve, owner, key, scale, flags, outValue, outSlope). The EE
            # source declarations follow the register layout of direct EE
            # callers instead. Give the host definitions the table's actual
            # callback signature while keeping the implementation logic.
            curve_callbacks = (
                ("TangentWeightCurve", "EvaluateTangentWeightCurve"),
                ("ControlPointWeightCurve", "EvaluateControlPointWeightCurve"),
                ("LinearWeightCurve", "EvaluateLinearWeightCurve"),
            )
            for curve_type, function in curve_callbacks:
                pattern = (
                    rf"void {function}\({curve_type}\* self, s32 unused, "
                    rf"s32 flags, f32\* outF, f32\* outI, f32 key, f32 scale\)")
                replacement = (
                    f"void {function}({curve_type}* self, PlacedObject* owner, "
                    "f32 key, f32 scale, s32 flags, f32* outF, f32* outI)")
                adapted, replacements = re.subn(pattern, replacement, adapted)
                if replacements != 1:
                    raise RuntimeError("expected one host weight-curve callback: " +
                                       function)
            # PositionCurveEval also carries key/scale before mode and output
            # pointers. The source method declarations follow EE register
            # residue, which shifts the output pointer onto the host stack's
            # scale word when called through a curve vtable.
            old = ("void EvaluateAxisPositionCurve(AxisPositionCurve* self, "
                   "PlacedObject* other, s32 c, s32 e, s32 f, f32 p12, f32 p13)\n{\n"
                   "    EvaluateAxisCurves(self->axis, other, c, &self->base, e, f, p12, p13);")
            new = ("void EvaluateAxisPositionCurve(AxisPositionCurve* self, "
                   "PlacedObject* other, f32 key, f32 scale, s32 mode, "
                   "void* outValue, void* outSlope)\n{\n"
                   "    EvaluateAxisCurves(self->axis, other, mode, &self->base, "
                   "(f32*)outValue, (f32*)outSlope, key, scale);")
            if adapted.count(old) != 1:
                raise RuntimeError("expected one host axis-position callback")
            adapted = adapted.replace(old, new, 1)
            adapted = adapted.replace(
                "((VectorCurveToldFn)other->desc02()->m28)(other, e, f);",
                "((VectorCurveToldFn)other->desc02()->m28)(other, "
                "(s32)outValue, (s32)outSlope);")
            old = ("void EvaluateVectorKeyPositionCurve(VectorKeyPositionCurve* self, "
                   "PlacedObject* other, s32 a2, s32 a3, s32 a4, f32 p12, f32 p13)\n{\n"
                   "    EvaluateVectorKeyCurve(self->count, (s32)self->keys, a2, a3, a4, p12, p13);")
            new = ("void EvaluateVectorKeyPositionCurve(VectorKeyPositionCurve* self, "
                   "PlacedObject* other, f32 key, f32 scale, s32 mode, "
                   "void* outValue, void* outSlope)\n{\n"
                   "    EvaluateVectorKeyCurve(self->count, (s32)self->keys, mode, "
                   "(s32)outValue, (s32)outSlope, key, scale);")
            if adapted.count(old) != 1:
                raise RuntimeError("expected one host keyed-position callback")
            adapted = adapted.replace(old, new, 1)
            adapted = adapted.replace(
                "((VectorCurveToldFn)other->desc02()->m28)(other, a3, a4);",
                "((VectorCurveToldFn)other->desc02()->m28)(other, "
                "(s32)outValue, (s32)outSlope);")
            # RotationCurveEval supplies the key and scale after its two
            # output pointers. The source body relied on EE FPR residue when
            # forwarding to EvaluateAxisCurves; make those values explicit on
            # the host stack ABI.
            old = ("void EvaluateAxisRotationCurve(AxisRotationCurve* self, "
                   "PlacedObject* other, s32 c, void* p3, void* p4)\n{\n"
                   "    AngleSample tmp;\n"
                   "    EvaluateAxisCurves(self->axis, other, c, &self->base, "
                   "p3 ? (void*)&tmp : 0, p4 ? (void*)tmp.b : 0);")
            new = ("void EvaluateAxisRotationCurve(AxisRotationCurve* self, "
                   "PlacedObject* other, s32 c, void* p3, void* p4, "
                   "f32 key, f32 scale)\n{\n"
                   "    AngleSample tmp;\n"
                   "    EvaluateAxisCurves(self->axis, other, c, &self->base, "
                   "p3 ? (f32*)&tmp : 0, p4 ? (f32*)tmp.b : 0, key, scale);")
            if adapted.count(old) != 1:
                raise RuntimeError("expected one host rotation-curve callback")
            adapted = adapted.replace(old, new, 1)
            # The matching Hermite implementation at 0x001303F0 takes its
            # output pointers first. Reorder the two source calls that express
            # the same register flow as scalar inputs followed by outputs.
            adapted = adapted.replace(
                "void EvaluateHermiteSegment(f32 delta, f32 diff, f32 scale, "
                "f32 p4, f32 p14, f32 p1C, f32 p24, f32* outF, f32* outI, "
                "s32 zero);",
                "void EvaluateHermiteSegment(f32* outF, f32* outI, f32* out2, "
                "f32 delta, f32 diff, f32 scale, f32 p4, f32 p14, f32 p1C, "
                "f32 p24);")
            adapted, replacements = re.subn(
                r"EvaluateHermiteSegment\(delta, diff, scale, "
                r"\(\(TangentWeightKey\*\)p\)->value, "
                r"\(\(TangentWeightKey\*\)p\)->f, "
                r"\(\(TangentWeightKey\*\)p \+ 1\)->value, "
                r"\(\(TangentWeightKey\*\)p \+ 1\)->d, outF, outI, 0\);",
                "EvaluateHermiteSegment(outF, outI, 0, delta, diff, scale, "
                "((TangentWeightKey*)p)->value, ((TangentWeightKey*)p)->f, "
                "((TangentWeightKey*)p + 1)->value, "
                "((TangentWeightKey*)p + 1)->d);",
                adapted)
            adapted, second = re.subn(
                r"EvaluateHermiteSegment\(left, dt, 1\.0f, "
                r"owner->f224->f90, owner->f224->f94, value, slope, "
                r"&owner->f224->f90, &owner->f224->f94, 0\);",
                "EvaluateHermiteSegment(&owner->f224->f90, "
                "&owner->f224->f94, 0, left, dt, 1.0f, owner->f224->f90, "
                "owner->f224->f94, value, slope);",
                adapted)
            if replacements != 1 or second != 1:
                raise RuntimeError("expected two Hermite register-order calls in " +
                                   rel.as_posix())
        elif rel.as_posix() == "main/script_machine.cpp":
            # EE GCC 2.95 emits the intended reverse destruction loop when the
            # pointer walks from a neighboring local into arr. Modern GCC
            # treats comparing pointers from separate objects as undefined
            # behavior and optimizes the loop into an infinite walk. Preserve
            # the same 32-element reverse order with defined array indexing.
            old = """    p = &t;
    while (arr != p) {
        p--;
        DestroyScriptValue(p, 0);
    }"""
            new = """    for (s32 cleanupIndex = 32; cleanupIndex != 0; --cleanupIndex) {
        DestroyScriptValue(&arr[cleanupIndex - 1], 0);
    }"""
            if adapted.count(old) != 1:
                raise RuntimeError("expected EvalScriptCall cleanup loop in " +
                                   rel.as_posix())
            adapted = adapted.replace(old, new, 1)
        elif rel.as_posix() == "main/game_progress.cpp":
            # SetCurrentLevelEntry carries the WorldKeyBlock in an EE argument
            # register through GetEntryIdByKey and FindKeyEntry. Give that
            # value an explicit host parameter at both calls. The decomp
            # currently represents the helpers as zero-argument functions,
            # which is valid for EE register conventions but makes the host
            # ABI read an unrelated stack word as the key pointer.
            replacements = [
                ("s32 GetEntryIdByKey(s32 a0);",
                 "s32 GetEntryIdByKey(KeyedEntry* key);"),
                ("KeyedEntry* FindKeyEntry(void);",
                 "KeyedEntry* FindKeyEntry(KeyedEntry* key);"),
                ("s32 GetEntryIdByKey(void);",
                 "s32 GetEntryIdByKey(KeyedEntry* key);"),
                ("s32 GetEntryIdByKey(void)\n{\n    s32 v;\n    KeyedEntry* r = FindKeyEntry();",
                 "s32 GetEntryIdByKey(KeyedEntry* key)\n{\n    s32 v;\n    KeyedEntry* r = FindKeyEntry(key);")]
            for old, new in replacements:
                if adapted.count(old) != 1:
                    raise RuntimeError("expected one keyed-entry ABI site in " +
                                       rel.as_posix() + ": " + old.splitlines()[0])
                adapted = adapted.replace(old, new, 1)
        elif rel.as_posix() == "main/base_object.cpp":
            # The reworked matching source now owns both helpers. Its second
            # overload calls the first with output pointers last, while the
            # implementation at 0x00183F40 takes them first. Preserve the
            # intended function and make that argument order explicit for the
            # host compiler; the former adapter targeted a retired backfill.
            adapted, replacements = re.subn(
                r"FitLineThroughPoints\(x, b \* 0\.003921569f, y, "
                r"d \* 0\.003921569f, out0, out1\);",
                "FitLineThroughPoints(out0, (f32*)out1, x, "
                "b * 0.003921569f, y, d * 0.003921569f);",
                adapted)
            if replacements != 1:
                raise RuntimeError("expected one line-fit overload call in " +
                                   rel.as_posix())
            # The EE compiler carries the world's first argument in a register
            # into TakeCallRecord even though this source omits it. Native x86
            # does not preserve that convention, so make it explicit in the
            # staged copy of AddCallbackNode.
            adapted, declaration_count = re.subn(
                r"(?m)^void\* TakeCallRecord\(void\);$",
                "void* TakeCallRecord(void* world);", adapted)
            adapted, call_count = re.subn(
                r"(Callback01\* n = \(Callback01\*\)TakeCallRecord)\(\);",
                r"\1(world);", adapted)
            if declaration_count != 1 or call_count != 1:
                raise RuntimeError("expected callback pool world carryover in " +
                                   rel.as_posix())
            if args.trace_functions:
                # The Splash graph emits script events through this shared
                # path. Record whether event 0x11/0xE exists, queues, and
                # actually runs for graph 0x25F. This observes the retail
                # script path without changing its decisions.
                trace_decl = 'extern "C" void sly1_port_trace_value(u32, u32);\n'
                adapted = trace_decl + adapted
                trigger = "void TriggerScriptEvent(BaseObject* self, s32 idx, s32 flag, s32* out)\n{"
                trigger_trace = trigger + """
    const s32 trace_graph = self != 0 && self->desc != 0 &&
        *(s32*)((u8*)self->desc + 4) == 0x88 && self->id == 0x25F;
    if (trace_graph) {
        sly1_port_trace_value(0x183a0001u, (u32)idx);
        sly1_port_trace_value(0x183a0002u, (u32)flag);
        sly1_port_trace_value(0x183a0003u, (u32)self->f2C);
        sly1_port_trace_value(0x183a0004u, (u32)out);
    }
"""
                if adapted.count(trigger) != 1:
                    raise RuntimeError("expected TriggerScriptEvent definition")
                adapted = adapted.replace(trigger, trigger_trace, 1)
                lookup_miss = """    if (self->f2C == 0 || LookupScriptVariable(self->f2C, idx, 1, &r) == 0) {
        DestroyScriptValue(&r, 2);"""
                lookup_miss_trace = """    if (self->f2C == 0 || LookupScriptVariable(self->f2C, idx, 1, &r) == 0) {
        if (trace_graph) sly1_port_trace_value(0x183a0005u, (u32)idx);
        DestroyScriptValue(&r, 2);"""
                if adapted.count(lookup_miss) != 1:
                    raise RuntimeError("expected graph script lookup miss path")
                adapted = adapted.replace(lookup_miss, lookup_miss_trace, 1)
                queue_call = """        AppendQueuedCall(self->world, (void*)(void (*)(void*, s32, void*))RunScriptEventHandler, self, idx, a4);"""
                queue_trace = """        if (trace_graph) sly1_port_trace_value(0x183a0006u, (u32)idx);
""" + queue_call
                if adapted.count(queue_call) != 1:
                    raise RuntimeError("expected graph script queue call")
                adapted = adapted.replace(queue_call, queue_trace, 1)
                handler = "void RunScriptEventHandler(BaseObject* self, s32 key, R11B008Node* list)\n{"
                handler_trace = handler + """
    const s32 trace_graph = self != 0 && self->desc != 0 &&
        *(s32*)((u8*)self->desc + 4) == 0x88 && self->id == 0x25F;
    if (trace_graph) {
        sly1_port_trace_value(0x183b0001u, (u32)key);
        sly1_port_trace_value(0x183b0002u, (u32)self->f2C);
    }
"""
                if adapted.count(handler) != 1:
                    raise RuntimeError("expected RunScriptEventHandler definition")
                adapted = adapted.replace(handler, handler_trace, 1)
                eval_body = "    EvalScriptBody(&q3, rec->body, slot);"
                eval_trace = """    if (trace_graph) sly1_port_trace_value(0x183b0003u, (u32)key);
""" + eval_body
                if adapted.count(eval_body) != 1:
                    raise RuntimeError("expected graph script body evaluation")
                adapted = adapted.replace(eval_body, eval_trace, 1)
        elif rel.as_posix() == "main/state_notifier.cpp" and args.trace_functions:
            # Log the retail notifier objects that select camera/state-graph
            # keys. This is diagnostic staging only; notifier decisions stay
            # in matching source.
            adapted = 'extern "C" void sly1_port_trace_value(u32, u32);\n' + adapted
            signature = "void SetStateNotifierState(Camera* self, s32 state)\n{"
            replacement = signature + """
    sly1_port_trace_value(0x147ab001u, (u32)self);
    sly1_port_trace_value(0x147ab002u, self != 0 ? (u32)self->desc : 0);
    sly1_port_trace_value(0x147ab003u, self != 0 ? (u32)self->id : 0);
    sly1_port_trace_value(0x147ab004u, (u32)state);
    sly1_port_trace_value(0x147ab005u, self != 0 ? (u32)CAMERA_AT(s32, self, 0x2D0) : 0);
    sly1_port_trace_value(0x147ab006u, self != 0 ? (u32)CAMERA_AT(s32, self, 0x2DC) : 0);
    sly1_port_trace_value(0x147ab007u, self != 0 ? (u32)CAMERA_AT(u32, self, 0x2E4) : 0);
    sly1_port_trace_value(0x147ab008u, self != 0 ? (u32)CAMERA_AT(u32, self, 0x2E8) : 0);
    sly1_port_trace_value(0x147ab009u, self != 0 ? (u32)CAMERA_AT(u32, self, 0x2EC) : 0);
    sly1_port_trace_value(0x147ab00au, self != 0 ? (u32)CAMERA_AT(u32, self, 0x334) : 0);
    sly1_port_trace_value(0x147ab00bu, self != 0 ? (u32)CAMERA_AT(u32, self, 0x338) : 0);
"""
            if adapted.count(signature) != 1:
                raise RuntimeError("expected SetStateNotifierState definition")
            adapted = adapted.replace(signature, replacement, 1)
            countdown_signature = "void SetCountdownNotifierState(CountdownNotifier* self, s32 mode)\n{"
            countdown_trace = countdown_signature + """
    sly1_port_trace_value(0x148ff001u, (u32)self);
    sly1_port_trace_value(0x148ff002u, self != 0 ? (u32)self->id : 0);
    sly1_port_trace_value(0x148ff003u, (u32)mode);
    sly1_port_trace_value(0x148ff004u, self != 0 ? (u32)self->state : 0);
    sly1_port_trace_value(0x148ff005u, self != 0 ? (u32)self->f338 : 0);
    sly1_port_trace_value(0x148ff006u, self != 0 ? (u32)self->f2E8 : 0);
"""
            if adapted.count(countdown_signature) != 1:
                raise RuntimeError("expected SetCountdownNotifierState definition")
            adapted = adapted.replace(countdown_signature, countdown_trace, 1)
            notification_signature = (
                "void HandleCountdownNotifierNotification(CountdownNotifier* self, "
                "s32 kind, void* msg)\n{")
            notification_trace = notification_signature + """
    sly1_port_trace_value(0x148e4001u, (u32)self);
    sly1_port_trace_value(0x148e4002u, self != 0 ? (u32)self->id : 0);
    sly1_port_trace_value(0x148e4003u, (u32)kind);
    sly1_port_trace_value(0x148e4004u, (u32)msg);
    sly1_port_trace_value(0x148e4005u, self != 0 ? (u32)self->f2E8 : 0);
"""
            if adapted.count(notification_signature) != 1:
                raise RuntimeError("expected countdown notification handler")
            adapted = adapted.replace(notification_signature, notification_trace, 1)
        elif rel.as_posix() == "main/file_1F2758.cpp" and args.trace_functions:
            # Staging-only trace of subgroup completion after Continue.
            adapted = 'extern "C" void sly1_port_trace_value(u32, u32);\n' + adapted
            find_signature = "ObjectSubGroup* FindActiveObjectSubGroup(void)\n{"
            if adapted.count(find_signature) != 1:
                raise RuntimeError("expected active subgroup lookup")
            find_start = adapted.index(find_signature)
            find_end = adapted.index("\n}\n", find_start)
            find_body = adapted[find_start:find_end]
            find_return = "    return found;"
            if find_body.count(find_return) != 1:
                raise RuntimeError("expected active subgroup result")
            find_body = find_body.replace(find_return, """    static u32 trace_count;
    if (trace_count++ < 24) {
        sly1_port_trace_value(0x1f437801u, (u32)found);
        sly1_port_trace_value(0x1f437802u, found != 0 ? (u32)found->f2D0 : 0);
        sly1_port_trace_value(0x1f437803u, found != 0 ? (u32)found->f2E4 : 0);
        sly1_port_trace_value(0x1f437804u, found != 0 ? *(u32*)&found->f31C : 0);
        sly1_port_trace_value(0x1f437805u, found != 0 ? (u32)found->count : 0);
    }
    return found;""")
            adapted = adapted[:find_start] + find_body + adapted[find_end:]

            finish_signature = "void StartObjectSubGroupFinish(ObjectSubGroup* self)\n{"
            finish_trace = finish_signature + """
    static u32 trace_count;
    if (trace_count++ < 24) {
        sly1_port_trace_value(0x1f480801u, (u32)self);
        sly1_port_trace_value(0x1f480802u, self != 0 ? (u32)self->f2D0 : 0);
        sly1_port_trace_value(0x1f480803u, self != 0 ? (u32)self->f2E4 : 0);
        sly1_port_trace_value(0x1f480804u, self != 0 ? *(u32*)&self->f31C : 0);
        sly1_port_trace_value(0x1f480805u, self != 0 ? (u32)self->count : 0);
        sly1_port_trace_value(0x1f480806u, self != 0 ? (u32)self->f2D8 : 0);
    }
"""
            if adapted.count(finish_signature) != 1:
                raise RuntimeError("expected subgroup finish implementation")
            adapted = adapted.replace(finish_signature, finish_trace, 1)

            update_signature = "void UpdateObjectSubGroup(ObjectSubGroup* self)\n{"
            update_trace = update_signature + """
    static u32 trace_count;
    if (self != 0 && self->f2E4 == 5 && trace_count++ < 24) {
        sly1_port_trace_value(0x1f4c0101u, (u32)self);
        sly1_port_trace_value(0x1f4c0102u, (u32)self->f2D0);
        sly1_port_trace_value(0x1f4c0103u, (u32)self->f2E4);
        sly1_port_trace_value(0x1f4c0104u, *(u32*)&self->f31C);
        sly1_port_trace_value(0x1f4c0105u, *(u32*)&g_gameClock.time);
        sly1_port_trace_value(0x1f4c0106u, (u32)self->count);
    }
"""
            if adapted.count(update_signature) != 1:
                raise RuntimeError("expected subgroup update implementation")
            adapted = adapted.replace(update_signature, update_trace, 1)
        elif rel.as_posix() == "main/file_180EB0.cpp":
            # The EE compiler accepts implicit signed/unsigned vector
            # conversions for this helper; host GCC requires matching types.
            adapted = adapted.replace(
                "SetMatrixRows(Mat02& m, const s128& a, const s128& b, const s128& c)",
                "SetMatrixRows(Mat02& m, const u128& a, const u128& b, const u128& c)")
        # func_001612F8 writes a GIF packet from two 64-bit tag arguments.
        # Several matching translation units declared those arguments as
        # 32-bit values, which shifts the final output pointer on the i386
        # host ABI. Normalize only staged prototypes; leave decomp source
        # untouched and retain each unit's first pointer type.
        adapted = re.sub(
            r"(?m)^(void func_001612F8\([^,]+),[^;\n]*;",
            r"\1, u64 prim, s32 nloop, s32 nreg, u64 data, s32* out);",
            adapted)
        # Multiple recovered declarations of the same GIF packet helper used
        # incompatible widths (and one used void*). One host ABI signature is
        # required so 64-bit GIF tags do not shift the optional output pointer.
        adapted = re.sub(
            r"(?m)^[^\n]*\bReserveGifPackedData\([^;\n]*\);",
            "void ReserveGifPackedData(void*, u64, s32, s32, u64, s32*);",
            adapted)
        if rel.as_posix() in ("main/0015C000.cpp", "main/file_1549C8.cpp"):
            # The PS2 ABI gives long 64 bits; i386 gets the same aliases from
            # host_abi.h, where they are represented with long long.
            adapted = adapted.replace("typedef signed long s64;\n", "")
            adapted = adapted.replace("typedef unsigned long u64;\n", "")
        if rel.as_posix() == "main/0015C000.cpp":
            # The display worker publishes its current packet through a
            # shared global, then dereferences it twice while the main thread
            # may clear that global during a synchronous frame handoff. Keep
            # this iteration's pointer local, while preserving the published
            # pointer for the matching consumer.
            old_frame = "        D_0026231C = &D_00262320[D_00262314];"
            new_frame = (
                "        G262320* frame = &D_00262320[D_00262314];\n"
                "        D_0026231C = frame;"
            )
            if adapted.count(old_frame) != 1:
                raise RuntimeError("expected one display frame publication in " + rel.as_posix())
            adapted = adapted.replace(old_frame, new_frame)
            for member in ("i0C", "i04"):
                old_use = "((G262320*)D_0026231C)->" + member
                if adapted.count(old_use) != 1:
                    raise RuntimeError("expected one display frame read " + member +
                                       " in " + rel.as_posix())
                adapted = adapted.replace(old_use, "frame->" + member)
            # The memory-card cleanup iterates an EE list containing valid
            # low-address nodes. Translate those pointers to the host's
            # mapped low-memory shadow before dereferencing them.
            old = (
                "        Node_15EFE8* obj = *(Node_15EFE8**)it->link;\n"
                "        it->link = (u8*)obj + it->offset;\n"
                "        return obj;"
            )
            new = (
                "        Node_15EFE8* obj = *(Node_15EFE8**)it->link;\n"
                "        u32 address = (u32)obj;\n"
                "        if (address != 0 && address < 0x10000)\n"
                "            obj = (Node_15EFE8*)(address + 0x10000);\n"
                "        it->link = (u8*)obj + it->offset;\n"
                "        return obj;"
            )
            if old not in adapted:
                raise RuntimeError("expected low-address list iterator in " + rel.as_posix())
            adapted = adapted.replace(old, new)
        elif rel.as_posix() == "main/001E8000.cpp":
            # Retail's trigonometry helper receives the angle in the first
            # argument slot, followed by the sine and cosine output pointers.
            # The decomp body models the mixed EE GPR/FPU argument placement
            # as (outputs, angle); on i386 that C++ signature reverses the
            # actual host stack layout used by its angle-first callers.
            old_signature = "void func_001EAD88(f32* sinOut, f32* cosOut, f32 angle)"
            new_signature = "void func_001EAD88(f32 angle, f32* sinOut, f32* cosOut)"
            if adapted.count(old_signature) != 1:
                raise RuntimeError("expected one EAD88 definition in " + rel.as_posix())
            adapted = adapted.replace(old_signature, new_signature)
        elif rel.as_posix() == "main/file_1C05D0.cpp":
            # The PS2 compiler lays each variadic argument in an 8-byte slot.
            # Native i386 callers use 4-byte slots, so stage the equivalent
            # host vararg walk without changing the decompilation source.
            old = "ap = (VaList_1C2430)__builtin_next_arg(size) - (8 - __builtin_args_info(0)) * 8;"
            if old not in adapted:
                raise RuntimeError("expected PS2 varargs expression not found in " + rel.as_posix())
            adapted = adapted.replace(old, "ap = (VaList_1C2430)__builtin_next_arg(size);")
            adapted = adapted.replace(
                "#define VA_ARG_1C2430(ap, T) (*(T*)((ap += 8) - 8))",
                "#define VA_ARG_1C2430(ap, T) (*(T*)((ap += sizeof(T)) - sizeof(T)))")
        elif rel.as_posix() == "lib/001FC000.cpp":
            # The retail library uses the SN Systems exception runtime. This
            # freestanding host launcher has no compatible unwind runtime;
            # route only its exceptional paths through Sly's registered
            # non-returning handlers. Ordinary allocation and library code is
            # left intact in staged source.
            begin = "void func_001FEC10(s32 count, void** allowed)\n{"
            end = "\n}\n\nvoid func_001FEDC8()"
            start_at = adapted.find(begin)
            end_at = adapted.find(end, start_at)
            if start_at < 0 or end_at < 0:
                raise RuntimeError("expected Sly exception handler not found in " + rel.as_posix())
            adapted = (adapted[:start_at] +
                       "void func_001FEC10(s32 count, void** allowed)\n{\n"
                       "    (void)count;\n    (void)allowed;\n    func_001FE938();\n" +
                       adapted[end_at:])
            adapted = adapted.replace("throw Fault_1FEDC8();", "func_001FE8F0();")
            adapted = adapted.replace("throw Fault_1FEE48();", "func_001FE8F0();")
            adapted = adapted.replace(" throw(Fault_1FD8A8)", "")
        elif rel.as_posix() == "lib/00200000.cpp":
            # These three matching functions are PS2 DMA/interrupt MMIO
            # boundaries. Keep their vanilla implementations in the staged
            # source for reference, but let separate port-native service
            # definitions own the callable retail addresses.
            for address in ("00201A18", "002027C8", "00202A80"):
                pattern = re.compile(r"\bfunc_" + address + r"(?=\s*\()")
                adapted, replacements = pattern.subn("decomp_func_" + address, adapted)
                if replacements != 1 and not (files_table.is_file() and replacements == 0):
                    raise RuntimeError("expected one platform function " + address +
                                       " in " + rel.as_posix())
            # The native display path replaces these two additional GS
            # register boundaries while keeping the original C bodies staged.
            for signature, address in (("void", "00201BB8"), ("s32", "00201C78")):
                pattern = re.compile(
                    r"(?m)^" + signature + r" func_" + address + r"\((?:void\* self|void \*self)\)"
                )
                adapted, replacements = pattern.subn(
                    signature + " decomp_func_" + address + "(void* self)", adapted)
                if replacements != 1 and not (files_table.is_file() and replacements == 0):
                    raise RuntimeError("expected one GS boundary definition " + address +
                                       " in " + rel.as_posix())
            # EE DMA wait routines poll CHCR until hardware clears STR. A
            # native DMA submission is synchronous, so port services own these
            # waits too; leave their matching bodies visible in staged C++.
            for address, pattern in (
                ("00202B68", r"(?m)^void func_00202B68\(DmaChan_202B68\* ch\)"),
                ("00202C38", r"(?m)^s32 func_00202C38\(DmaChan_202C38\* ch, s32 mode, s32 timeout\)"),
            ):
                signatures = {
                    "00202B68": "void decomp_func_00202B68(DmaChan_202B68* ch)",
                    "00202C38": "s32 decomp_func_00202C38(DmaChan_202C38* ch, s32 mode, s32 timeout)",
                }
                adapted, replacements = re.subn(pattern, signatures[address], adapted)
                if replacements != 1 and not (files_table.is_file() and replacements == 0):
                    raise RuntimeError("expected one DMA wait definition " + address +
                                       " in " + rel.as_posix())
            # Native CD reads complete synchronously. Preserve the matching
            # IOP status routine for reference, while the port reports its
            # host read result at this retail address.
            adapted, replacements = re.subn(
                r"(?m)^s32 func_00203D90\(void\)(?=\n\{)",
                "s32 decomp_func_00203D90(void)", adapted)
            if replacements != 1 and not (files_table.is_file() and replacements == 0):
                raise RuntimeError("expected one CD status definition in " + rel.as_posix())
        elif rel.as_posix() == "main/00184000.cpp":
            # The candidate projection helper has a shifted argument list.
            # Preserve that body in staging while the port supplies the
            # argument flow derived from the retail call site.
            adapted, replacements = re.subn(
                r"(?m)^void func_001840F0\(f32 a, f32 b, f32 c, f32 d, Out_1840F0\* out0, void\* out1\)",
                "void decomp_func_001840F0(f32 a, f32 b, f32 c, f32 d, Out_1840F0* out0, void* out1)",
                adapted)
            if replacements != 1 and not (files_table.is_file() and replacements == 0):
                raise RuntimeError("expected one projection helper in " + rel.as_posix())
            # Retail Quad16 inputs are 16-byte aligned. Native retail-address
            # data symbols can be only 8-byte aligned, so keep the source body
            # staged and replace its aligned vector load in the port layer.
            adapted, replacements = re.subn(
                r"(?m)^void func_001847F0\(C51\* self, Quad_001847F0\* src\)",
                "void decomp_func_001847F0(C51* self, Quad_001847F0* src)",
                adapted)
            if replacements != 1 and not (files_table.is_file() and replacements == 0):
                raise RuntimeError("expected one camera vector setter in " + rel.as_posix())
        elif rel.as_posix() == "main/001EC000.cpp":
            # The matching packet-copy body calls the packet-size helper with
            # an incomplete prototype. Keep it staged while a port-only writer
            # reproduces the retail VIF header and payload copy.
            adapted, replacements = re.subn(
                r"(?m)^void func_001EE668\(void\* s, void\* a1, s32 size, u32\* src, void\* a4\)",
                "void decomp_func_001EE668(void* s, void* a1, s32 size, u32* src, void* a4)",
                adapted)
            if replacements != 1 and not (files_table.is_file() and replacements == 0):
                raise RuntimeError("expected one packet-copy definition in " + rel.as_posix())
        elif rel.as_posix() == "main/001E4000.cpp":
            # G275F80 is a 0x40-byte native view assembled from separate retail
            # labels. Reserve the full view in the host image so its tail cannot
            # overlap the next compiled global (D_00274118) when callbacks write it.
            adapted, replacements = re.subn(
                r"(?m)^s32 D_00275F80 = 0;$",
                "u8 D_00275F80[0x40] = {0};", adapted)
            if replacements != 1 and not (files_table.is_file() and replacements == 0):
                raise RuntimeError("expected the 4-byte D_00275F80 retail label in " +
                                   rel.as_posix())
            # func_00140D90 clears this system table through +0x42f, although
            # the decompilation exposes only its first 0x210 bytes as a label.
            # Reserve the whole native span so the clear cannot hit a neighbor.
            adapted, replacements = re.subn(
                r"(?m)^u8 D_00261420\[528\] = \{0\};$",
                "u8 D_00261420[0x430] = {0};", adapted)
            if replacements != 1 and not (files_table.is_file() and replacements == 0):
                raise RuntimeError("expected the D_00261420 retail table label in " +
                                   rel.as_posix())
            # Retail labels can expose only part of a T217D object (some methods
            # write large derived arrays beyond those label extents). Reserve
            # each complete typed object in the host image to prevent adjacent
            # compiled globals from being overwritten during native startup.
            object_table = (source / "src" / "main" / "001A8000.cpp").read_text(
                encoding="utf-8")
            table = re.search(
                r"extern void\* const D_002486B0\[38\]\s*=\s*\{(.*?)\};",
                object_table, re.S)
            if table is None:
                raise RuntimeError("expected the T217D object table in 001A8000.cpp")
            typed_header = (source / "include" / "t217d.h").read_text(encoding="utf-8")
            object_types = {
                name: type_name
                for type_name, name in re.findall(
                    r"extern\s+(T217D_[0-9A-Fa-f]+)\s+(D_002[0-9A-Fa-f]+);",
                    typed_header)
            }
            adapted = adapted.replace('#include "common.h"',
                                      '#include "common.h"\n#include "t217d.h"', 1)
            for name in re.findall(r"&\s*(D_002[0-9A-Fa-f]+)", table.group(1)):
                if name not in object_types:
                    raise RuntimeError("missing typed T217D object " + name)
                object = re.compile(r"(?m)^[^\n]*\b" + re.escape(name) +
                                    r"\b[^\n]*=\s*\{(?:0)?\};(?:\s*//[^\n]*)?$")
                match = object.search(adapted)
                if match is None:
                    raise RuntimeError("expected zeroed T217D storage for " + name)
                adapted = (adapted[:match.start()] + object_types[name] + " " +
                           name + " = {};" + adapted[match.end():])
        elif rel.as_posix() == "main/00168000.cpp":
            # Native DMA completion is synchronous, so the display setup path
            # must not wait for the PS2 GS interrupt semaphore. Keep the retail
            # body staged while the port supplies its own no-wait equivalent.
            adapted, replacements = re.subn(
                r"(?m)^void func_001683B8\(void\)(?=\s*\{)",
                "void decomp_func_001683B8(void)", adapted)
            if replacements != 1 and not (files_table.is_file() and replacements == 0):
                raise RuntimeError("expected one GS completion wait in " + rel.as_posix())
        elif rel.as_posix() == "main/file_15F678.cpp":
            # Obj_161958 overlays the V24A0 packet writer with a C++ virtual
            # method, but the native host cannot use the retail buffer-end word
            # as a C++ vptr. Keep that body staged under a private name while a
            # port-only V24A0-aware implementation owns the callable address.
            adapted, replacements = re.subn(
                r"(?m)^    void func_00161958\(s32 count\);",
                "    void decomp_func_00161958(s32 count);", adapted)
            if replacements != 1 and not (files_table.is_file() and replacements == 0):
                raise RuntimeError("expected Obj_161958 declaration in " + rel.as_posix())
            adapted, replacements = re.subn(
                r"(?m)^void Obj_161958::func_00161958\(s32 count\)",
                "void Obj_161958::decomp_func_00161958(s32 count)", adapted)
            if replacements != 1 and not (files_table.is_file() and replacements == 0):
                raise RuntimeError("expected Obj_161958 method definition in " + rel.as_posix())
            if args.trace_functions:
                # Diagnose corrupt model mesh pointers without changing retail
                # behavior. The invalid value is reported at parse time and
                # again at the attachment DMA consumer, which separates a bad
                # stream/global base from later record corruption.
                # These model-reader bodies moved from file_15F678.cpp to
                # placed_object_model.cpp in Wave 33. Inject only at a body
                # that exists in this source unit so both source snapshots
                # remain traceable.
                hook = 'extern "C" void sly1_port_trace_value(u32, u32);\n'
                if "strip->mesh = mesh;" in adapted or "            switch (*c->f18) {" in adapted:
                    adapted = hook + adapted
                needle = "                strip->mesh = mesh;"
                if adapted.count(needle) == 1:
                    replacement = (
                        needle + '\n'
                        '                if ((u32)mesh < 0x10000u || (u32)mesh > 0x09000000u) {\n'
                        '                    sly1_port_trace_value(0x16620001u, (u32)mesh);\n'
                        '                    sly1_port_trace_value(0x16620002u, (u32)g_animRecords);\n'
                        '                }')
                    adapted = adapted.replace(needle, replacement, 1)
                needle = "            switch (*c->f18) {"
                if adapted.count(needle) == 1:
                    replacement = (
                        '            if ((u32)c->f18 < 0x10000u || (u32)c->f18 > 0x09000000u) {\n'
                        '                sly1_port_trace_value(0x1662a801u, (u32)c);\n'
                        '                sly1_port_trace_value(0x1662a802u, (u32)c->f18);\n'
                        '            }\n' + needle)
                    adapted = adapted.replace(needle, replacement, 1)
        elif rel.as_posix() == "main/state_walker.cpp" and args.trace_functions:
            # Follow serialized state-graph animation IDs through object
            # creation and into playback. This is diagnostics only; it makes
            # no change to the staged state machine or its decisions.
            adapted = ('extern "C" void sly1_port_trace_value(u32, u32);\n' + adapted)
            graph_load_end = """    for (j = 0; j < self->edgeCount; j++) {
        StateEdge* e = &self->edges[j];
        if (e->anim != 0) {
            s32 kind = ReadBufferedS16(s);
            s32 b = ReadBufferedS16(s);
            s32 c = ReadBufferedS16(s);
            BaseObject* o = CreateTypedRecord(kind, self->world, self->parent, b, c);
            e->anim = (Animation*)o;
            o->desc->m0A(o, s);
            ResolveAndStartObject(o);
        }
    }
}

void StartStateGraph"""
            graph_trace = """
    if (self->id == 0x25F) {
        sly1_port_trace_value(0x1b250001u, (u32)self);
        sly1_port_trace_value(0x1b250002u, (u32)self->id);
        sly1_port_trace_value(0x1b250003u, (u32)self->f34);
        sly1_port_trace_value(0x1b250004u, (u32)self->edgeCount);
        sly1_port_trace_value(0x1b250005u, (u32)self->armed);
        for (s32 k = 0; k < self->f34; ++k) {
            sly1_port_trace_value(0x1b251000u + (u32)k * 2u,
                                  (u32)self->f38[k].key);
            sly1_port_trace_value(0x1b251001u + (u32)k * 2u,
                                  (u32)self->f38[k].extra);
        }
        for (s32 k = 0; k < self->edgeCount; ++k) {
            StateEdge* edge = &self->edges[k];
            const u32 tag = 0x1b252000u + (u32)k * 4u;
            sly1_port_trace_value(tag, (u32)edge->a);
            sly1_port_trace_value(tag + 1u, (u32)edge->b);
            sly1_port_trace_value(tag + 2u, edge->flags);
            sly1_port_trace_value(tag + 3u,
                                  edge->anim != 0 ? (u32)edge->anim->id : 0xFFFFFFFFu);
        }
    }
"""
            graph_load_traced = graph_load_end.replace(
                "\n}\n\nvoid StartStateGraph",
                "\n" + graph_trace + "}\n\nvoid StartStateGraph")
            if graph_load_traced == graph_load_end:
                raise RuntimeError("expected state-graph loader end")
            if adapted.count(graph_load_end) != 1:
                raise RuntimeError("expected one completed state-graph loader")
            adapted = adapted.replace(graph_load_end, graph_load_traced, 1)
            start_graph_signature = "void StartStateGraph(StateGraph* self)\n{"
            start_graph_trace = start_graph_signature + """
    if (self->id == 0x25F) {
        sly1_port_trace_value(0x1b255001u, (u32)self);
        sly1_port_trace_value(0x1b255002u, (u32)self->armed);
        sly1_port_trace_value(0x1b255003u, (u32)self->f34);
        sly1_port_trace_value(0x1b255004u, (u32)self->edgeCount);
    }
"""
            if adapted.count(start_graph_signature) != 1:
                raise RuntimeError("expected StartStateGraph definition")
            adapted = adapted.replace(start_graph_signature, start_graph_trace, 1)
            needle = "            BaseObject* o = CreateTypedRecord(kind, self->world, self->parent, b, c);"
            replacement = (
                needle + '\n'
                '            sly1_port_trace_value(0x1b5f5001u, (u32)kind);\n'
                '            sly1_port_trace_value(0x1b5f5002u, (u32)o);')
            if adapted.count(needle) != 1:
                raise RuntimeError("expected state-graph animation record creation")
            adapted = adapted.replace(needle, replacement)
            needle = "                CreateAnimationPlayerWithoutPresetTargets(e->anim, self->f1C, self->f20, &self->player, fa, fb);"
            replacement = (
                '                sly1_port_trace_value(0x1b66a001u, (u32)e->anim);\n'
                '                sly1_port_trace_value(0x1b66a002u, (u32)self->owner);\n' + needle + '\n'
                '                sly1_port_trace_value(0x1b66a003u, (u32)self->player);\n'
                '                sly1_port_trace_value(0x1b66a004u, (u32)(self->player != 0 ? self->player->anim : 0));\n'
                '                sly1_port_trace_value(0x1b66a005u, (u32)(self->player != 0 && self->player->anim != 0 ? self->player->anim->world : 0));')
            if adapted.count(needle) != 1:
                raise RuntimeError("expected state-walker animation player creation")
            adapted = adapted.replace(needle, replacement)
            target_signature = "void SetStateWalkerTarget(StateWalker* self, s32 key)\n{"
            target_trace = target_signature + """
    if (self->owner != 0 && self->owner->id == 0x25F) {
        sly1_port_trace_value(0x1b253001u, (u32)self);
        sly1_port_trace_value(0x1b253002u, (u32)key);
        sly1_port_trace_value(0x1b253003u, (u32)self->f28);
        sly1_port_trace_value(0x1b253004u, (u32)self->f2C);
        sly1_port_trace_value(0x1b253005u, (u32)self->index);
        sly1_port_trace_value(0x1b253006u, (u32)self->edge);
        sly1_port_trace_value(0x1b253007u, (u32)self->player);
    }
"""
            if adapted.count(target_signature) != 1:
                raise RuntimeError("expected SetStateWalkerTarget definition")
            adapted = adapted.replace(target_signature, target_trace, 1)
            next_edge_signature = "void StartNextStateWalkerEdge(StateWalker* self)\n{"
            next_edge_trace = next_edge_signature + """
    if (self->owner != 0 && self->owner->id == 0x25F) {
        sly1_port_trace_value(0x1b254001u, (u32)self);
        sly1_port_trace_value(0x1b254002u, (u32)self->f28);
        sly1_port_trace_value(0x1b254003u, (u32)self->f2C);
        sly1_port_trace_value(0x1b254004u, (u32)self->index);
        sly1_port_trace_value(0x1b254005u, (u32)self->edge);
        sly1_port_trace_value(0x1b254006u, (u32)self->player);
    }
"""
            if adapted.count(next_edge_signature) != 1:
                raise RuntimeError("expected StartNextStateWalkerEdge definition")
            adapted = adapted.replace(next_edge_signature, next_edge_trace, 1)
            state_change_signature = (
                "void NotifyStateWalkerChange(StateWalker* self, s32 idx1, s32 idx2)\n{")
            state_change_trace = state_change_signature + """
    if (self->owner != 0 && self->owner->id == 0x25F) {
        sly1_port_trace_value(0x1b256001u, (u32)self);
        sly1_port_trace_value(0x1b256002u, (u32)idx1);
        sly1_port_trace_value(0x1b256003u, (u32)idx2);
        sly1_port_trace_value(0x1b256004u,
            idx1 >= 0 && idx1 < self->owner->f34
                ? (u32)self->owner->f38[idx1].key : 0xFFFFFFFFu);
        sly1_port_trace_value(0x1b256005u,
            idx2 >= 0 && idx2 < self->owner->f34
                ? (u32)self->owner->f38[idx2].key : 0xFFFFFFFFu);
    }
"""
            if adapted.count(state_change_signature) != 1:
                raise RuntimeError("expected NotifyStateWalkerChange definition")
            adapted = adapted.replace(state_change_signature, state_change_trace, 1)
            chosen_edge = "            self->edge = e;\n            self->player = 0;"
            chosen_edge_trace = """            if (g->id == 0x25F) {
                sly1_port_trace_value(0x1b254010u, (u32)e->a);
                sly1_port_trace_value(0x1b254011u, (u32)e->b);
                sly1_port_trace_value(0x1b254012u, e->flags);
                sly1_port_trace_value(0x1b254013u,
                                      e->anim != 0 ? (u32)e->anim->id : 0xFFFFFFFFu);
                sly1_port_trace_value(0x1b254014u, (u32)self->f28);
                sly1_port_trace_value(0x1b254015u, (u32)self->index);
                sly1_port_trace_value(0x1b254016u, (u32)self->f2C);
            }
""" + chosen_edge
            if adapted.count(chosen_edge) != 1:
                raise RuntimeError("expected one selected state-graph edge")
            adapted = adapted.replace(chosen_edge, chosen_edge_trace, 1)
        elif rel.as_posix() == "main/placed_object.cpp" and args.trace_functions:
            # Record the exact arguments and class fields at the animation
            # player boundary. This distinguishes a malformed Animation
            # record from a call-site passing a different object class.
            adapted = ('extern "C" void sly1_port_trace_value(u32, u32);\n' + adapted)
            needle = (
                "void CreateAnimationPlayer(Animation* self, Animation* alt, s32 npairs, "
                "TargetBinding* pairs, s32 flags, AnimationPlayer** out, f32 fa, f32 fb)\n{\n")
            replacement = needle + (
                "    sly1_port_trace_value(0x12c28801u, (u32)self);\n"
                "    sly1_port_trace_value(0x12c28802u, (u32)__builtin_return_address(0));\n"
                "    sly1_port_trace_value(0x12c28803u, self != 0 ? (u32)self->desc : 0);\n"
                "    sly1_port_trace_value(0x12c28804u, self != 0 ? (u32)self->id : 0);\n"
                "    sly1_port_trace_value(0x12c28805u, self != 0 ? (u32)self->world : 0);\n"
                "    sly1_port_trace_value(0x12c28806u, self != 0 ? (u32)self->trackCount : 0);\n"
                "    sly1_port_trace_value(0x12c28807u, self != 0 ? (u32)self->tracks : 0);\n"
                "    sly1_port_trace_value(0x12c28808u, self != 0 ? (u32)self->targetCount : 0);\n"
                "    sly1_port_trace_value(0x12c28809u, self != 0 ? (u32)self->targets : 0);\n"
                "    sly1_port_trace_value(0x12c2880au, (u32)alt);\n"
                "    sly1_port_trace_value(0x12c2880bu, (u32)npairs);\n"
                "    sly1_port_trace_value(0x12c2880cu, (u32)pairs);\n")
            if adapted.count(needle) != 1:
                raise RuntimeError("expected CreateAnimationPlayer definition")
            adapted = adapted.replace(needle, replacement, 1)
            needle = (
                "    for (s32 m = 0; m < self->startCount; m++) {\n"
                "        RunAnimationEvent(obj, &self->startList[m], &tmp);\n"
                "    }")
            replacement = (
                "    for (s32 m = 0; m < self->startCount; m++) {\n"
                "        sly1_port_trace_value(0x12c28820u, (u32)m);\n"
                "        sly1_port_trace_value(0x12c28821u, (u32)self->startList[m].kind);\n"
                "        RunAnimationEvent(obj, &self->startList[m], &tmp);\n"
                "        sly1_port_trace_value(0x12c28822u, (u32)m);\n"
                "    }")
            if adapted.count(needle) != 1:
                raise RuntimeError("expected animation start event loop")
            adapted = adapted.replace(needle, replacement, 1)
            needle = "    TriggerScriptEvent(self, 9, 1, &pobj);"
            replacement = (
                "    sly1_port_trace_value(0x12c28823u, (u32)self);\n" + needle + "\n"
                "    sly1_port_trace_value(0x12c28824u, (u32)self);")
            if adapted.count(needle) != 1:
                raise RuntimeError("expected animation player script notification")
            adapted = adapted.replace(needle, replacement, 1)
            needle = (
                "s32 CreateAnimationPlayerWithoutPresetTargets(void* a, void* b, s32 c, s32 d)\n{\n")
            replacement = needle + (
                "    sly1_port_trace_value(0x12c7d801u, (u32)__builtin_return_address(0));\n"
                "    sly1_port_trace_value(0x12c7d802u, (u32)a);\n"
                "    sly1_port_trace_value(0x12c7d803u, (u32)b);\n"
                "    sly1_port_trace_value(0x12c7d804u, (u32)c);\n"
                "    sly1_port_trace_value(0x12c7d805u, (u32)d);\n")
            if adapted.count(needle) != 1:
                raise RuntimeError("expected CreateAnimationPlayer wrapper")
            adapted = adapted.replace(needle, replacement, 1)
            needle = "void* CreateAnimationPlayerReturned(void* self, s32 key, s32 a2)\n{\n"
            replacement = needle + (
                "    sly1_port_trace_value(0x12c7d821u, (u32)__builtin_return_address(0));\n"
                "    sly1_port_trace_value(0x12c7d822u, (u32)self);\n"
                "    sly1_port_trace_value(0x12c7d823u, (u32)key);\n"
                "    sly1_port_trace_value(0x12c7d824u, (u32)a2);\n")
            replacement += (
                "    volatile u32* trace_frame = (volatile u32*)__builtin_frame_address(0);\n"
                "    for (u32 i = 0; i < 8; ++i) {\n"
                "        sly1_port_trace_value(0x12c7d830u + i, trace_frame[i]);\n"
                "    }\n")
            if adapted.count(needle) != 1:
                raise RuntimeError("expected returned animation-player wrapper")
            adapted = adapted.replace(needle, replacement, 1)
        elif rel.as_posix() == "main/placed_object_model.cpp" and args.trace_functions:
            # Wave 33 moved this model-reader body out of file_15F678.cpp.
            # Keep the invalid strip pointer trace attached to its new owner.
            adapted = ('extern "C" void sly1_port_trace_value(u32, u32);\n' + adapted)
            needle = "                strip->mesh = mesh;"
            replacement = (
                needle + '\n'
                '                if ((u32)mesh < 0x10000u || (u32)mesh > 0x09000000u) {\n'
                '                    sly1_port_trace_value(0x16620001u, (u32)mesh);\n'
                '                    sly1_port_trace_value(0x16620002u, (u32)g_animRecords);\n'
                '                }')
            if adapted.count(needle) != 1:
                raise RuntimeError("expected Wave 33 strip mesh assignment")
            adapted = adapted.replace(needle, replacement, 1)
            needle = "            switch (*c->f18) {"
            replacement = (
                '            if ((u32)c->f18 < 0x10000u || (u32)c->f18 > 0x09000000u) {\n'
                '                sly1_port_trace_value(0x1662a801u, (u32)c);\n'
                '                sly1_port_trace_value(0x1662a802u, (u32)c->f18);\n'
                '            }\n' + needle)
            if adapted.count(needle) != 1:
                raise RuntimeError("expected Wave 33 attachment DMA kind switch")
            adapted = adapted.replace(needle, replacement, 1)
        elif rel.as_posix() == "main/object_host_functions.cpp" and args.trace_functions:
            # Identify the script host-call object that reaches the returned
            # animation-player wrapper. The declared Animation* can be a
            # misleading alias for a different runtime class.
            adapted = ('extern "C" void sly1_port_trace_value(u32, u32);\n' + adapted)
            needle = (
                "ScriptValue* CreateScriptAnimationPlayer(ScriptValue* ret, Animation* obj, "
                "s32 unused, ScriptValue* args)\n{")
            replacement = needle + (
                "\n    sly1_port_trace_value(0x12c7d811u, (u32)__builtin_return_address(0));"
                "\n    sly1_port_trace_value(0x12c7d812u, (u32)obj);"
                "\n    sly1_port_trace_value(0x12c7d813u, obj != 0 ? (u32)obj->desc : 0);"
                "\n    sly1_port_trace_value(0x12c7d814u, obj != 0 ? (u32)obj->id : 0);"
            )
            if adapted.count(needle) != 1:
                raise RuntimeError("expected script animation-player host function")
            adapted = adapted.replace(needle, replacement, 1)
        elif rel.as_posix() == "main/script_machine.cpp" and args.trace_functions:
            # Catch script builtin/host dispatch around the animation helper.
            # Native helpers filter by target or receiver class, keeping normal
            # script execution quiet in the startup trace.
            hook = ('extern "C" void sly1_native_trace_script_host_call(u32, u32, u32);\n'
                    'extern "C" void sly1_native_trace_script_builtin(u32, u32, u32, u32);\n')
            adapted = hook + adapted
            needle = "        ent->fn(&t, count, arr, scope);"
            replacement = (
                "        sly1_native_trace_script_builtin((u32)ent->fn, (u32)count, "
                "(u32)arr, (u32)scope);\n" + needle)
            if adapted.count(needle) != 1:
                raise RuntimeError("expected script builtin function dispatch")
            adapted = adapted.replace(needle, replacement, 1)
            needle = "        host->fn(&t, host->self, count, arr);"
            replacement = (
                "        sly1_native_trace_script_host_call((u32)host->fn, "
                "(u32)host->self, (u32)count);\n" + needle)
            if adapted.count(needle) != 1:
                raise RuntimeError("expected script host function dispatch")
            adapted = adapted.replace(needle, replacement, 1)
        elif rel.as_posix() == "main/file_184D78.cpp":
            # Retain the most recent one-based startup callback index in the
            # native launcher diagnostics. This isolates a startup spin loop
            # without instrumenting every retail function's return path.
            callback_declaration = (
                'extern "C" void sly1_native_startup_callback(unsigned int);\n\n'
                'void func_00185B38(void)\n{')
            adapted, replacements = re.subn(
                r"(?m)^void func_00185B38\(void\)\n\{",
                callback_declaration, adapted)
            if replacements != 1 and not (files_table.is_file() and replacements == 0):
                raise RuntimeError("expected startup callback loop in " + rel.as_posix())
            adapted, replacements = re.subn(
                r"(?m)^(\s*)tbl\[i\]\(\);",
                r"\1sly1_native_startup_callback(i);\n\1tbl[i]();", adapted)
            if replacements != 1 and not (files_table.is_file() and replacements == 0):
                raise RuntimeError("expected one startup callback call in " + rel.as_posix())
            # Replace only the IOP module/CDVD bootstrap at its retail address;
            # keep the original matching body in the staged source for review.
            adapted, replacements = re.subn(
                r"(?m)^void func_001858F8\(\)(?=\n\{)",
                "void decomp_func_001858F8()", adapted)
            if replacements != 1 and not (files_table.is_file() and replacements == 0):
                raise RuntimeError("expected one IOP bootstrap definition in " +
                                   rel.as_posix())
            # Native DMA channels live in host memory; this wait routine must
            # recover the channel record if startup retained its retail MMIO
            # address in the shared global.
            adapted, replacements = re.subn(
                r"(?m)^void func_00185A58\(void\)(?=\n\{)",
                "void decomp_func_00185A58(void)", adapted)
            if replacements != 1 and not (files_table.is_file() and replacements == 0):
                raise RuntimeError("expected one DMA wait definition in " + rel.as_posix())
            adapted, replacements = re.subn(
                r"(?m)^void func_00185AC8\(\)(?=\n\{)",
                "void decomp_func_00185AC8()", adapted)
            if replacements != 1 and not (files_table.is_file() and replacements == 0):
                raise RuntimeError("expected one second DMA wait definition in " +
                                   rel.as_posix())
        elif rel.as_posix() == "main/00134000.cpp":
            # The decomp declares this wrapper void even though callers use
            # its status result and the wrapped initializer returns s32. Keep
            # the correction in the native staging layer so the decomp tree
            # remains untouched and can replace it once its signature is fixed.
            adapted, replacements = re.subn(
                r"(?m)^void func_00137328\(void\* a, void\* b, s32 c\);\n\n"
                r"void func_001373B0\(void\* a, S18D410\* pair\)\n\{\n"
                r"    func_00137328\(a, pair->p0, pair->second\);\n\}",
                "s32 func_00137328(void* a, void* b, s32 c);\n\n"
                "s32 func_001373B0(void* a, S18D410* pair)\n{\n"
                "    return func_00137328(a, pair->p0, pair->second);\n}",
                adapted)
            if replacements != 1 and not (files_table.is_file() and replacements == 0):
                raise RuntimeError("expected mismatched resource-key wrapper in " +
                                   rel.as_posix())
        elif rel.as_posix() in {
            "main/file_1E08F8.cpp", "main/file_1E2420.cpp",
            "main/on_off_object.cpp",
        } and args.trace_functions:
            # Capture the pending key/blob and current world when the direct
            # native startup path handles a resource request.
            signature = "void RunPendingWorldLoad(WorldLoadRequest* self)\n{"
            if signature in adapted:
                hook = ('extern "C" void sly1_native_trace_resource_state(u32, u32, u32);\n'
                        'extern "C" void sly1_native_trace_startup(u32, u32, u32);\n')
                adapted = hook + adapted
                replacement = (
                    signature + "\n"
                    "    sly1_native_trace_startup(0xFFFFFFFDu, 0, 0);\n"
                    "    sly1_native_trace_resource_state((u32)self, (u32)self->key, "
                    "(u32)D_00275710);"
                )
                if adapted.count(signature) != 1:
                    raise RuntimeError("expected one resource request handler in " + rel.as_posix())
                adapted = adapted.replace(signature, replacement)
                end = "    self->state = 0;\n}\n\nvoid RequestWorldLoadByMode"
                end_replacement = (
                    "    self->state = 0;\n"
                    "    sly1_native_trace_startup(0xFFFFFFFDu, 1, 0);\n"
                    "}\n\nvoid RequestWorldLoadByMode"
                )
                if adapted.count(end) != 1:
                    raise RuntimeError("expected world-load completion in " + rel.as_posix())
                adapted = adapted.replace(end, end_replacement, 1)
        elif rel.as_posix() == "main/memory_card.cpp" and args.trace_functions:
            hook = ('extern "C" void sly1_native_trace_memory_update(u32, u32, u32, u32, float, float, u32, s32);\n')
            adapted = hook + adapted
            signature = "void UpdateMemoryCard(MemoryCard* self)\n{"
            replacement = (
                signature + "\n"
                "    sly1_native_trace_memory_update(0, (u32)self->state, "
                "(u32)self->targetState, (u32)self->displayMode, "
                "g_elapsedTime[0], self->stateTime, (u32)g_pauseMenuElement.open, "
                "g_pauseMenuElement.page);")
            if adapted.count(signature) != 1:
                raise RuntimeError("expected UpdateMemoryCard definition")
            adapted = adapted.replace(signature, replacement, 1)
            needle = "    }\n}\n\ns32 CompareMemoryCardEntryTimes"
            replacement = (
                "    }\n"
                "    sly1_native_trace_memory_update(1, (u32)self->state, "
                "(u32)self->targetState, (u32)self->displayMode, "
                "g_elapsedTime[0], self->stateTime, (u32)g_pauseMenuElement.open, "
                "g_pauseMenuElement.page);\n"
                "}\n\ns32 CompareMemoryCardEntryTimes")
            if adapted.count(needle) != 1:
                raise RuntimeError("expected UpdateMemoryCard end")
            adapted = adapted.replace(needle, replacement, 1)
        elif rel.as_posix() == "main/main_loop.cpp" and args.trace_functions:
            # Mark the retail constructor phase and each startup initializer.
            # A single guest callback can wait indefinitely, so the before /
            # after pair identifies the exact callback without global
            # function instrumentation, which changes several EE ABI cases.
            hook = ('#include "screen_element.h"\n'
                    '#include "background_job.h"\n'
                    'extern void* const g_screenElements[];\n'
                    'extern s32 g_padRouting;\n'
                    'extern s32 g_padSelectFlags;\n'
                    'extern s32 D_002701B8;\n'
                    'extern "C" void sly1_native_trace_startup(u32, u32, u32);\n'
                    'extern "C" void sly1_native_trace_frame_stage(u32);\n'
                    'extern "C" void sly1_native_trace_memory_gate(u32, u32, u32, u32, u32, u32, u32, u32, u32, u32);\n'
                    'extern "C" void sly1_native_trace_memory_timing(float, float, float);\n'
                    'extern "C" void sly1_native_trace_frame_state(u32, u32, u32, u32, u32, u32, u32, u32, u32, u32, u32, u32, u32, u32, u32, u32, u32, u32, u32, u32, u32, u32);\n'
                    'extern "C" void sly1_native_host32_wait_frame(void);\n')
            adapted = hook + adapted
            loop = "void RunMainLoop(s32 a, s32 b)\n{"
            if adapted.count(loop) != 1:
                raise RuntimeError("expected RunMainLoop definition")
            adapted = adapted.replace(loop, loop + "\n    sly1_native_trace_frame_stage(0);", 1)
            frame_stages = (
                ("        UpdatePadConnection(&g_padRecord);", 1),
                ("            UpdateCameraLocation(g_camera);", 2),
                ("            RunWorldFrameLists(g_currentWorld, g_camera);", 3),
                ("            QueueScreenElementDraws(g_elementStack);", 4),
                ("            DrawAllScreenElements(g_elementStack);", 5),
                ("            SubmitRenderFrame();", 6),
            )
            for statement, stage in frame_stages:
                if adapted.count(statement) != 1:
                    raise RuntimeError("expected RunMainLoop stage " + str(stage))
                adapted = adapted.replace(
                    statement,
                    "            sly1_native_trace_frame_stage(" + str(stage) + ");\n" + statement
                    if stage > 1 else
                    "        sly1_native_trace_frame_stage(" + str(stage) + ");\n" + statement,
                    1)
            end_frame = "            SubmitRenderFrame();\n        }\n        g_frameCount++;"
            end_frame_replacement = (
                "            SubmitRenderFrame();\n"
                "            sly1_native_trace_frame_stage(7);\n"
                "        }\n"
                "        g_frameCount++;\n"
                "        if ((g_frameCount % 60) == 0) {\n"
                "            u32 visible_ui = 0;\n"
                "            u32 text_ui = 0;\n"
                "            for (u32 i = 0; i < 37; ++i) {\n"
                "                const u32 element = (u32)g_screenElements[i];\n"
                "                if (element < 0x00800000u || element >= 0x00900000u) continue;\n"
                "                if (*(volatile s32*)(element + 0x250u) != 0) ++visible_ui;\n"
                "                if (*(volatile u8*)(element + 0x08u) != 0) ++text_ui;\n"
                "            }\n"
                "            volatile u32* stack = (volatile u32*)g_elementStack;\n"
                "            u32 stack_top = stack[11] > 0 && stack[11] <= 8 ? stack[3 + stack[11] - 1] : 0;\n"
                "            u32 stack_top_vt = 0, stack_top_mode = 0, stack_top_text = 0;\n"
                "            if (stack_top >= 0x00200000u && stack_top < 0x09000000u) {\n"
                "                volatile u32* item = (volatile u32*)stack_top;\n"
                "                stack_top_vt = item[0];\n"
                "                stack_top_text = *(volatile u8*)(stack_top + 0x08u);\n"
                "                stack_top_mode = *(volatile u32*)(stack_top + 0x250u);\n"
                "            }\n"
                "            sly1_native_trace_frame_state((u32)g_currentWorld, stack[1], stack[11],\n"
                "                stack[12], (u32)g_worldLoadRequest, (u32)g_moviePlayer.queued,\n"
                "                visible_ui, text_ui, (u32)g_padRecord.f14, (u32)g_padRecord.f18,\n"
                "                (u32)g_padRecord.flags, (u32)g_pauseMenuElement.open,\n"
                "                (u32)g_pauseMenuElement.page, (u32)g_pauseMenuElement.mode,\n"
                "                (u32)g_screenFade.mode, stack_top, stack_top_vt,\n"
                "                stack_top_mode, stack_top_text, (u32)g_padRouting,\n"
                "                (u32)g_padSelectFlags, (u32)g_blankPadRecord.f14);\n"
                "            sly1_native_trace_memory_gate((u32)g_memoryCard.state,\n"
                "                (u32)g_memoryCard.targetState, (u32)g_memoryCard.displayMode,\n"
                "                (u32)g_memoryCard.i4F44, (u32)D_002701B8, stack[1],\n"
                "                (u32)g_backgroundJob.thread, (u32)g_backgroundJob.job,\n"
                "                (u32)g_backgroundJob.result, (u32)g_backgroundJob.semaphore);\n"
                "            sly1_native_trace_memory_timing(g_gameClock.elapsed,\n"
                "                g_memoryCard.stateTime, g_memoryCard.displayModeTime);\n"
                "        }\n"
                "        sly1_native_host32_wait_frame();\n"
                "        sly1_native_trace_frame_stage(8);")
            if adapted.count(end_frame) != 1:
                raise RuntimeError("expected RunMainLoop frame completion")
            adapted = adapted.replace(end_frame, end_frame_replacement, 1)
            before = "    RunConstructorsOnce();"
            if adapted.count(before) != 1:
                raise RuntimeError("expected RunConstructorsOnce in " + rel.as_posix())
            adapted = adapted.replace(
                before,
                "    sly1_native_trace_startup(0xFFFFFFFFu, 0, 0);\n" + before +
                "\n    sly1_native_trace_startup(0xFFFFFFFFu, 1, 0);",
                1)
            before = "    RunStartupInitializers();"
            if adapted.count(before) != 1:
                raise RuntimeError("expected RunStartupInitializers in " + rel.as_posix())
            adapted = adapted.replace(
                before,
                "    sly1_native_trace_startup(0xFFFFFFFEu, 0, 0);\n" + before +
                "\n    sly1_native_trace_startup(0xFFFFFFFEu, 1, 0);",
                1)
            before = "        tbl[i]();"
            if adapted.count(before) != 1:
                raise RuntimeError("expected startup callback invocation in " + rel.as_posix())
            adapted = adapted.replace(
                before,
                "        sly1_native_trace_startup(i, 0, (u32)tbl[i]);\n" + before +
                "\n        sly1_native_trace_startup(i, 1, (u32)tbl[i]);",
                1)
        elif rel.as_posix() == "main/0011C000.cpp":
            # Sound RPC requests cannot be serviced without Sly's IOP sound
            # module. Keep the matching body staged while the port supplies a
            # synchronous, silent native service at this retail address.
            adapted, replacements = re.subn(
                r"(?m)^s32 func_0011D968\(s32 id, s32 size, u8\* data\)(?=\n\{)",
                "s32 decomp_func_0011D968(s32 id, s32 size, u8* data)", adapted)
            if replacements != 1 and not (files_table.is_file() and replacements == 0):
                raise RuntimeError("expected one sound RPC definition in " + rel.as_posix())
            # The native port has no Sly IOP sound module. Keep the matching
            # bank loader available in staged source while the port supplies
            # a silent, synchronous implementation at its retail address.
            adapted, replacements = re.subn(
                r"(?m)^s32 func_0011D140\(s32 a0, s32 a1\)(?=\n\{)",
                "s32 decomp_func_0011D140(s32 a0, s32 a1)", adapted)
            if replacements != 1 and not (files_table.is_file() and replacements == 0):
                raise RuntimeError("expected one sound bank loader definition in " + rel.as_posix())
        elif rel.as_posix() == "lib/00204000.cpp":
            # Replace the IOP RPC wait with the native CD service; retain the
            # retail body in staged source so it remains available for review.
            adapted, replacements = re.subn(
                r"(?m)^s32 func_002040E8\(void\* arg\)(?=\n\{)",
                "s32 decomp_func_002040E8(void* arg)", adapted)
            if replacements != 1 and not (files_table.is_file() and replacements == 0):
                raise RuntimeError("expected one IOP service-start definition in " +
                                   rel.as_posix())
        elif (rel.as_posix() == "lib/remote_call.cpp" or
              re.search(r"(?m)^s32 (?:func_001F80C0|BindRemoteService)\(", adapted)):
            # SIF RPC completion for the two sound bootstrap services has no
            # IOP peer in the host build. Keep the matching implementation in
            # staged source, and let a port-only synchronous boundary own the
            # retail address.
            adapted, replacements = re.subn(
                r"(?m)^s32 (?:func_001F80C0\(Ctx_1F80C0\* self, s32 a1, s32 a2\)|"
                r"BindRemoteService\(RemoteCallClient\* self, u32 cmd, s32 mode\))(?=\n\{)",
                lambda match: match.group(0).replace("func_001F80C0", "decomp_func_001F80C0")
                .replace("BindRemoteService", "decomp_BindRemoteService"), adapted)
            # The latest reorganization moved this address into the shared
            # remote_call.cpp unit. Match its Ctx and named method forms, but
            # only in the unit whose config/files.tsv range owns 0x1F80C0.
            if replacements == 0 and rel.as_posix() == "lib/remote_call.cpp":
                adapted, replacements = re.subn(
                    r"(?m)^s32 BindRemoteService\(RemoteCallClient\* self, u32 cmd, s32 mode\)(?=\n\{)",
                    "s32 decomp_BindRemoteService(RemoteCallClient* self, u32 cmd, s32 mode)",
                    adapted)
            if replacements != 1 and not (files_table.is_file() and replacements == 0):
                raise RuntimeError("expected one SIF RPC definition in " + rel.as_posix())
        elif rel.as_posix() == "main/00150000.cpp":
            # Native processes cannot map valid EE low-memory addresses below
            # the host mmap_min_addr. Keep the matching list-offset helper in
            # the staged source and let a port-only boundary translate those
            # low results into the corresponding mapped guest-RAM shadow.
            adapted, replacements = re.subn(
                r"(?m)^u32 func_00152460\(List01\* p, u32 value\)(?=\n\{)",
                "u32 func_00152460(List01* p, u32 value);\n"
                "u32 decomp_func_00152460(List01* p, u32 value)", adapted)
            if replacements != 1 and not (files_table.is_file() and replacements == 0):
                raise RuntimeError("expected one list address helper in " + rel.as_posix())
            adapted, replacements = re.subn(
                r"(?m)^void func_00152470\(List01\* self, u32 value\)(?=\n\{)",
                "void func_00152470(List01* self, u32 value);\n"
                "void decomp_func_00152470(List01* self, u32 value)", adapted)
            if replacements != 1 and not (files_table.is_file() and replacements == 0):
                raise RuntimeError("expected one list append definition in " + rel.as_posix())
        elif rel.as_posix() == "main/001B4000.cpp":
            # The renderer's first cleanup can run before a world exists.
            # Keep the matching pool insertion body staged and let the native
            # boundary safely ignore returns to the absent world's pool.
            adapted, replacements = re.subn(
                r"(?m)^void func_001B5F28\(Pool6C\* list, T2195\* item\)(?=\n\{)",
                "void decomp_func_001B5F28(Pool6C* list, T2195* item)", adapted)
            if replacements != 1 and not (files_table.is_file() and replacements == 0):
                raise RuntimeError("expected one world pool insertion in " + rel.as_posix())
        elif rel.as_posix() == "main/00200000.cpp":
            # The retail DMA table contains MMIO addresses. The native port
            # owns separate host channel records, so route this lookup through
            # the port service while retaining the matching body in staging.
            adapted, replacements = re.subn(
                r"(?m)^u32 func_002027A0\(u32 index\) \{[^\n]*\}",
                "u32 decomp_func_002027A0(u32 index) { if (index >= 10) return 0; return D_00279340[index]; }",
                adapted)
            if replacements != 1 and not (files_table.is_file() and replacements == 0):
                raise RuntimeError("expected one DMA channel lookup in " + rel.as_posix())
        if rel.as_posix() == "main/effect_spawner.cpp":
            # The decomp models ReadBufferedU32 as a one-word aggregate in this
            # unit. On the i386 SysV host that adds a hidden result pointer,
            # but the retail reader returns the word in EAX. Keep the same
            # four bytes while matching the actual host call ABI.
            declaration = "StreamWord ReadBufferedU32(void* src);"
            declarations = adapted.count(declaration)
            if declarations != 1:
                raise RuntimeError("expected one stream-word reader declaration in " +
                                   rel.as_posix())
            adapted = adapted.replace(declaration, "s32 ReadBufferedU32(void* src);")
            local = "StreamWord tmp = ReadBufferedU32(src);"
            locals_ = adapted.count(local)
            if locals_ != 1:
                raise RuntimeError("expected one stream-word reader use in " + rel.as_posix())
            adapted = adapted.replace(local, "s32 tmp = ReadBufferedU32(src);")
        if args.trace_functions and rel.as_posix() == "main/effect_spawner.cpp":
            # Identify a malformed nested class read slot before the retail
            # virtual dispatch in LoadGroupMembers faults.
            hook = ('extern "C" void sly1_native_trace_group_child(\n'
                    '    u32, u32, u32, u32);\n'
                    'extern "C" void sly1_native_trace_path_curve(\n'
                    '    u32, u32, u32, u32);\n'
                    'extern "C" void sly1_native_trace_capped_list(\n'
                    '    u32, u32, u32, u32);\n')
            adapted = hook + adapted
            old = """        BaseObject* obj = (BaseObject*)CreateTypedRecord(r1, self->world, self->parent, r2, r3);
        s32 n;"""
            new = """        sly1_native_trace_group_child(0x4c475250u, (u32)self->world,
            (u32)self->parent, (u32)self->count);
        BaseObject* obj = (BaseObject*)CreateTypedRecord(r1, self->world, self->parent, r2, r3);
        sly1_native_trace_group_child((u32)r1, (u32)obj,
            obj != 0 ? (u32)obj->desc : 0,
            obj != 0 && obj->desc != 0 ? (u32)obj->desc->m0A : 0);
        s32 n;"""
            if adapted.count(old) != 1:
                raise RuntimeError("expected one group child read site in " + rel.as_posix())
            adapted = adapted.replace(old, new, 1)
            old = """        n->curve20 = CreatePathCurve(idx);
        if (n->curve20->vt->m00 != 0) {"""
            new = """        n->curve20 = CreatePathCurve(idx);
        sly1_native_trace_path_curve((u32)idx, (u32)n->curve20,
            n->curve20 != 0 ? (u32)n->curve20->vt : 0,
            n->curve20 != 0 && n->curve20->vt != 0 ? (u32)n->curve20->vt->m00 : 0);
        if (n->curve20->vt->m00 != 0) {"""
            if adapted.count(old) != 1:
                raise RuntimeError("expected one path curve reader site in " + rel.as_posix())
            adapted = adapted.replace(old, new, 1)
            # Instrument the canonical capped-list reader below once its actual
            # body is present in this whole-file unit.
            list_entry = """void ReadCappedWordList(CappedWordList* self, s32 count, void* unused, void* src)
{
"""
            if adapted.count(list_entry) != 1:
                raise RuntimeError("expected one capped word-list definition in " + rel.as_posix())
            adapted = adapted.replace(
                list_entry,
                list_entry + "    sly1_native_trace_capped_list(2, (u32)count, (u32)self, (u32)src);\n",
                1)
            end = """        }
    }
}

void LoadEffectSpawner"""
            end_new = """        }
    }
    sly1_native_trace_capped_list(3, (u32)count, (u32)self, (u32)src);
}

void LoadEffectSpawner"""
            if adapted.count(end) != 1:
                raise RuntimeError("expected capped-list end in " + rel.as_posix())
            adapted = adapted.replace(end, end_new, 1)
            callsite = """        ReadCappedWordList((u8*)m + 0x130, t, self, stream);"""
            callsite_new = """        sly1_native_trace_capped_list(1, (u32)t, (u32)((u8*)m + 0x130), (u32)stream);
        ReadCappedWordList((u8*)m + 0x130, t, self, stream);"""
            if adapted.count(callsite) != 1:
                raise RuntimeError("expected capped-list call site in " + rel.as_posix())
            adapted = adapted.replace(callsite, callsite_new, 1)
        if args.trace_functions and rel.as_posix() in {
                "main/00124000.cpp", "main/001B4000.cpp", "main/001B8000.cpp"}:
            # Global -finstrument-functions changes behavior where the decomp
            # has mismatched prototypes. Trace only the C02 root lifecycle at
            # selected boundaries so diagnostics do not perturb game calls.
            hook = 'extern "C" void sly1_native_trace_c02_state(u32, u32);\n'
            adapted = hook + adapted
            trace_points = {
                "main/00124000.cpp": [
                    ("void func_001240F8(C02* self)\n{",
                     "    sly1_native_trace_c02_state(0x1240F8, (u32)self);\n"),
                    ("        self->root = self;",
                     "        self->root = self;\n"
                     "        sly1_native_trace_c02_state(0x1240F9, (u32)self);"),
                    ("        self->root = ((C02*)self->parent)->root;",
                     "        self->root = ((C02*)self->parent)->root;\n"
                     "        sly1_native_trace_c02_state(0x1240FA, (u32)self);")],
                "main/001B4000.cpp": [
                    ("void func_001B7968(C03* self)\n{",
                     "    sly1_native_trace_c02_state(0x1B7968, (u32)self);\n"),
                    ("void func_001B7A98(C03* self)\n{",
                     "    sly1_native_trace_c02_state(0x1B7A98, (u32)self);\n"),
                    ("    func_001240F8(self);",
                     "    func_001240F8(self);\n"
                     "    sly1_native_trace_c02_state(0x1B7A99, (u32)self);")],
                "main/001B8000.cpp": [
                    ("void func_001B89B8(C03* self)\n{",
                     "    sly1_native_trace_c02_state(0x1B89B8, (u32)self);\n"),
                    ("    func_00125108(self);",
                     "    func_00125108(self);\n"
                     "    sly1_native_trace_c02_state(0x1B89B9, (u32)self);"),
                    ("        self->desc03()->m32(self);",
                     "        sly1_native_trace_c02_state(0x1BAC30, (u32)self);\n"
                     "        self->desc03()->m32(self);")]
            }[rel.as_posix()]
            for old, new in trace_points:
                if adapted.count(old) != 1:
                    raise RuntimeError("expected one C02 trace location in " +
                                       rel.as_posix() + ": " + old.splitlines()[0])
                adapted = adapted.replace(old, old + "\n" + new, 1)
        if args.trace_functions and rel.as_posix() == "main/00180000.cpp":
            adapted = 'extern "C" void sly1_native_trace_c02_state(u32, u32);\n' + adapted
            hook = 'extern "C" void sly1_native_trace_c02_list(u32, u32, u32, u32);\n'
            adapted = hook + adapted
            signature = "void func_00182C80(C01* self)\n{"
            if adapted.count(signature) != 1:
                raise RuntimeError("expected C01 parent-link constructor in " + rel.as_posix())
            adapted = adapted.replace(
                signature,
                signature + "\n    sly1_native_trace_c02_state(0x182C80, (u32)self);", 1)
            old = """    if (func_001526D0(sub, self) != 0) {
        return;
    }
    func_00152470(sub, self);
    if (func_00182FC8(self) != 0) {
        self->desc->m04(self);
    }"""
            new = """    s32 already_linked = func_001526D0(sub, self);
    sly1_native_trace_c02_list(0x182C80, (u32)sub, (u32)self,
                               (u32)already_linked);
    if (already_linked != 0) {
        return;
    }
    func_00152470(sub, self);
    s32 fully_linked = func_00182FC8(self);
    sly1_native_trace_c02_list(0x182FC8, (u32)sub, (u32)self,
                               (u32)fully_linked);
    if (fully_linked != 0) {
        self->desc->m04(self);
    }"""
            if adapted.count(old) != 1:
                raise RuntimeError("expected C01 constructor list gate in " + rel.as_posix())
            adapted = adapted.replace(old, new, 1)
        for address in backfill_addresses:
            names = {"func_" + address.upper()}
            if address in address_names:
                names.add(address_names[address])
            for name in names:
                definition = re.compile(
                    r"(?m)^(?P<header>\s*(?:[A-Za-z_][\w:*&<>]*\s+)+\b" +
                    re.escape(name) + r"\s*\([^{};]*\)[^{};]*)\s*(?=\{)")
                adapted = definition.sub(
                    lambda match: match.group("header").replace(
                        name, "decomp_" + name, 1) + " ", adapted)
        # Matching units declare AddGifRegisterWrite with inconsistent host
        # types. The retail ABI is (writer, 32-bit register, 64-bit value);
        # normalize every staged declaration and definition to that layout.
        # Without this, a u64 register shifts the value by one stack word on
        # i386 and GS writes such as SCISSOR_1 become corrupt.
        gif_writer_signature = re.compile(
            r"(?m)(void\s+AddGifRegisterWrite\s*\(\s*"
            r"(?:PacketWriter|void)\s*\*\s*\w+\s*,\s*)"
            r"(?:s32|u64)\s+(\w+)\s*,\s*"
            r"(?:s32|s64|u64)\s+(\w+)(\s*\))")
        adapted, normalized_gif_writers = gif_writer_signature.subn(
            r"\1s32 \2, u64 \3\4", adapted)
        if rel.as_posix() == "main/placed_object.cpp":
            # This matched non-void wrapper calls a void function and falls
            # through. GCC -O2 omits its epilogue, so host execution enters the
            # next retail function body with the previous arguments still on
            # the stack. Keep this host ABI adaptation in the generated copy;
            # the decompilation source remains unchanged.
            needle = "    CreateAnimationPlayer(a, b, 0, 0, c, d);\n}"
            replacement = "    CreateAnimationPlayer(a, b, 0, 0, c, d);\n    return 0;\n}"
            if adapted.count(needle) != 1:
                raise RuntimeError("expected host32 animation-player wrapper fallthrough")
            adapted = adapted.replace(needle, replacement, 1)
        staged.write_text(adapted, encoding="utf-8")
        unit_flags = flags.copy()
        if rel.as_posix() == "main/0016C000.cpp":
            # Avoid depending on the host's optional 32-bit libc headers.
            unit_flags = flags + ["-Dfabsf=__builtin_fabsf"]
        cmd = compiler + unit_flags + ["-I", str(src.parent), "-c", str(staged), "-o", str(obj)]
        proc = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        if proc.returncode:
            failures.append((rel.as_posix(), proc.stderr))
        else:
            warnings += proc.stderr.count("warning:")
            if proc.stderr:
                print(proc.stderr, file=sys.stderr, end="")

    backfill_objects = objects_root / "port_native"
    if backfill_objects.exists():
        shutil.rmtree(backfill_objects)
    backfills = sorted(backfill_root.rglob("*.cpp")) if backfill_root.exists() else []
    backfill_failures = []
    for src in backfills:
        rel = src.relative_to(backfill_root)
        obj = objects_root / "port_native" / rel.with_suffix(".o")
        obj.parent.mkdir(parents=True, exist_ok=True)
        cmd = compiler + flags + [
            "-I", str(port), "-I", str(port / "runtime" / "include"),
        ]
        if src.name == "sly1_native_loader_bridge.cpp":
            # The freestanding launcher has no 32-bit libc. Loader-stage
            # tracing is optional there; leave the bridge's native dispatch
            # active while compiling its trace hook out.
            cmd += ["-DSLY1_HOST32_FREESTANDING"]
        cmd += ["-c", str(src), "-o", str(obj)]
        proc = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        if proc.returncode:
            backfill_failures.append((src.relative_to(port).as_posix(), proc.stderr))
        else:
            warnings += proc.stderr.count("warning:")
            if proc.stderr:
                print(proc.stderr, file=sys.stderr, end="")

    print(f"Sly host C++ units: {len(sources) - len(failures)}/{len(sources)} compiled")
    print(f"Port-native backfills: {len(backfills) - len(backfill_failures)}/{len(backfills)} compiled; {warnings} compiler warnings")
    if failures or backfill_failures:
        for name, error in failures + backfill_failures:
            print(f"\n{name}:\n{error}", file=sys.stderr)
        return 1
    print("Skipped unsupported PS2 exception runtime units: " +
          ", ".join(sorted(unsupported_native_units)))
    print(f"staged sources: {staged_root}")
    print(f"object files: {objects_root}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
