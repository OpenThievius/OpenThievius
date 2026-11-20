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


def forward_load_collider_stream(text, label):
    """Make the EE's carried load-stream argument explicit in staged code."""
    calls = list(re.finditer(r"\bLoadCollider\(self\);", text))
    if not calls:
        return text

    declaration = re.compile(
        r"(?m)^(?P<indent>[ \t]*void\s+LoadCollider\()"
        r"(?P<arg>[^,;\n()]*)\)(?P<tail>\s*;)")
    declarations = list(declaration.finditer(text))
    if not declarations:
        raise RuntimeError("LoadCollider calls lack a declaration in " + label)

    definition = re.compile(
        r"(?m)^[ \t]*[A-Za-z_][\w:*&<> ]*?\s+"
        r"(?P<name>[A-Za-z_]\w*)\s*\((?P<args>[^{};]*)\)\s*\n[ \t]*\{")
    signature_edits = {}
    call_edits = []
    for call in calls:
        preceding = [m for m in definition.finditer(text, 0, call.start())]
        if not preceding:
            raise RuntimeError("cannot find LoadCollider caller in " + label)
        caller = preceding[-1]
        args = caller.group("args")
        if "," in args or re.search(r"\breader\b", args):
            raise RuntimeError("unexpected implicit-stream caller in " + label +
                               ": " + caller.group("name"))
        signature_edits[caller.start("args")] = (
            caller.end("args"), args + ", void* reader")
        call_edits.append((call.start(), call.end(), "LoadCollider(self, reader);"))

    edits = list(signature_edits.items())
    edits = [(start, end, value) for start, (end, value) in edits]
    edits.extend(call_edits)
    for start, end, value in sorted(edits, reverse=True):
        text = text[:start] + value + text[end:]

    # The declaration syntax varies between units (Collider* and void*), but
    # all single-argument forms represent the same EE load callback.
    text, changed = declaration.subn(
        lambda m: m.group("indent") + m.group("arg") +
        ", void* reader);", text)
    if changed != len(declarations):
        raise RuntimeError("failed to stage LoadCollider declarations in " + label)
    return text


def forward_placed_object_load_stream(text, label):
    """Forward the descriptor load stream through staged base-loader calls."""
    calls = list(re.finditer(
        r"\b(?P<base>LoadPlacedObject|LoadObjectSet)"
        r"\((?P<object>(?!void\b)[A-Za-z_]\w*)\);", text))
    if not calls:
        return text
    definition = re.compile(
        r"(?m)^[ \t]*[A-Za-z_][\w:*&<> ]*?\s+"
        r"(?P<name>[A-Za-z_]\w*)\s*\((?P<args>[^{};]*)\)\s*\{")
    function_edits = {}
    call_edits = []
    callback_names = set()
    for item in calls:
        preceding = list(definition.finditer(text, 0, item.start()))
        if not preceding:
            raise RuntimeError("cannot find descriptor loader in " + label)
        caller = preceding[-1]
        args = caller.group("args").strip()
        if "," in args:
            raise RuntimeError("unexpected base-loader parameters in " + label +
                               ": " + caller.group("name"))
        callback_names.add(caller.group("name"))
        function_edits[caller.start("args")] = (
            caller.end("args"), caller.group("args") + ", void* load_stream")
        call_edits.append((item.start(), item.end(),
                           item.group("base") + "(" + item.group("object") +
                           ", load_stream);"))
    edits = [(start, end, value)
             for start, (end, value) in function_edits.items()]
    edits.extend(call_edits)
    for start, end, value in sorted(edits, reverse=True):
        text = text[:start] + value + text[end:]

    for base in ("LoadPlacedObject", "LoadObjectSet"):
        prototype = re.compile(
            rf"(?m)^(?P<prefix>[ \t]*(?:extern\s+)?void\s+{base}\()"
            rf"(?P<arg>[^,;\n()]*)\)(?P<tail>\s*;)")
        def add_stream(m):
            arg = m.group("arg").strip()
            if not arg or arg == "void":
                return m.group(0)
            return (m.group("prefix") + m.group("arg") +
                    ", void* load_stream)" + m.group("tail"))
        text = prototype.sub(add_stream, text)

    for name in callback_names:
        prototype = re.compile(
            rf"(?m)^(?P<prefix>[ \t]*(?:extern\s+)?void\s+{re.escape(name)}\()"
            rf"(?P<arg>[^,;\n()]*)\)(?P<tail>\s*;)")
        text = prototype.sub(
            lambda m: (m.group(0) if not m.group("arg").strip() or
                       m.group("arg").strip() == "void" else
                       m.group("prefix") + m.group("arg") +
                       ", void* load_stream)" + m.group("tail")), text)
    return text


def forward_copy_callback_source(text, label):
    """Forward slot-9's source through base-copy calls in staged host code."""
    call = re.compile(
        r"\b(?P<base>CopyCollider|CopyPlacedObject|CopyObjectData)"
        r"\((?P<object>(?!void\b)[A-Za-z_]\w*)\);")
    calls = list(call.finditer(text))
    if not calls:
        return text

    definition = re.compile(
        r"(?m)^[ \t]*[A-Za-z_][\w:*&<> ]*?\s+"
        r"(?P<name>[A-Za-z_]\w*)\s*\((?P<args>[^{};]*)\)\s*\{")
    function_edits = {}
    call_edits = []
    callback_names = set()
    for item in calls:
        preceding = list(definition.finditer(text, 0, item.start()))
        if not preceding:
            raise RuntimeError("cannot find copy callback in " + label)
        caller = preceding[-1]
        args = caller.group("args").strip()
        if "," in args:
            raise RuntimeError("unexpected copy callback parameters in " +
                               label + ": " + caller.group("name"))
        name = caller.group("name")
        callback_names.add(name)
        function_edits[caller.start("args")] = (
            caller.end("args"), caller.group("args") + ", void* copy_source")
        call_edits.append((item.start(), item.end(),
                           item.group("base") + "(" + item.group("object") +
                           ", copy_source);"))

    edits = [(start, end, value)
             for start, (end, value) in function_edits.items()]
    edits.extend(call_edits)
    for start, end, value in sorted(edits, reverse=True):
        text = text[:start] + value + text[end:]

    for base in ("CopyCollider", "CopyPlacedObject", "CopyObjectData"):
        prototype = re.compile(
            rf"(?m)^(?P<prefix>[ \t]*(?:extern\s+)?void\s+{base}\()"
            rf"(?P<arg>[^,;\n()]*)\)(?P<tail>\s*;)")
        def add_base_source(m):
            arg = m.group("arg").strip()
            if not arg or arg == "void":
                return m.group(0)
            return (m.group("prefix") + m.group("arg") +
                    ", void* copy_source)" + m.group("tail"))
        text = prototype.sub(add_base_source, text)

    for name in callback_names:
        prototype = re.compile(
            rf"(?m)^(?P<prefix>[ \t]*(?:extern\s+)?void\s+{re.escape(name)}\()"
            rf"(?P<arg>[^,;\n()]*)\)(?P<tail>\s*;)")
        def add_callback_source(m):
            arg = m.group("arg").strip()
            if not arg or arg == "void":
                return m.group(0)
            return (m.group("prefix") + m.group("arg") +
                    ", void* copy_source)" + m.group("tail"))
        text = prototype.sub(add_callback_source, text)
    if label == "main/effect_spawner.cpp":
        old = "CopyPlacedObject(self, copy_source);\n    self->rec->uses++;"
        new = ("CopyPlacedObject(self, copy_source);\n"
               "    self->rec = ((EffectSpawner*)copy_source)->rec;\n"
               "    self->rec->uses++;")
        if text.count(old) != 1:
            raise RuntimeError("expected EffectSpawner shared-record copy in " + label)
        text = text.replace(old, new, 1)
    return text


def forward_animation_marker_arguments(text, label):
    """Make the EE-carried animation and key explicit in the host copy."""
    if label != "main/placed_object.cpp":
        return text
    current_form = (
        "AnimEvent* FindAnimationMarkerEvent(Animation* self, s32 key)",
        "f32 GetAnimationMarkerTime(Animation* self, s32 key)",
        "void MoveObjectAnimationTrackToEnd(Animation* a0, BaseObject* owner)",
        "FindEntryByObjectId((BlendAnimationView*)a0, owner)",
    )
    if all(text.count(item) == 1 for item in current_form):
        return text
    old_decl = "void* FindAnimationMarkerEvent(void);"
    if text.count(old_decl) != 1:
        raise RuntimeError("expected animation marker forward declaration")
    text = text.replace(old_decl, "", 1)
    old_signature = "f32 GetAnimationMarkerTime(void)"
    new_signature = "f32 GetAnimationMarkerTime(Animation* self, s32 key)"
    if text.count(old_signature) != 1:
        raise RuntimeError("expected animation marker wrapper")
    text = text.replace(old_signature, new_signature, 1)
    old_call = "(f32*)FindAnimationMarkerEvent();"
    new_call = "(f32*)FindAnimationMarkerEvent(self, key);"
    if text.count(old_call) != 1:
        raise RuntimeError("expected animation marker call")
    text = text.replace(old_call, new_call, 1)

    # MoveObjectAnimationTrackToEnd is called with the owner in a1 on EE and
    # carries that key into FindEntryByObjectId. Make both values explicit.
    old_move = "void MoveObjectAnimationTrackToEnd(Animation* a0)"
    new_move = "void MoveObjectAnimationTrackToEnd(Animation* a0, void* owner)"
    if text.count(old_move) != 1:
        raise RuntimeError("expected animation track move wrapper")
    text = text.replace(old_move, new_move, 1)
    old_lookup = "void* r = FindEntryByObjectId();"
    new_lookup = "void* r = FindEntryByObjectId((BlendAnimationView*)a0, owner);"
    if text.count(old_lookup) != 1:
        raise RuntimeError("expected animation track lookup")
    return text.replace(old_lookup, new_lookup, 1)


def forward_blend_entry_flag_arguments(text, label):
    """Forward the EE-carried blend view and object id to its lookup."""
    if label != "main/blend_animation.cpp":
        return text
    old_decl = "void* FindEntryByObjectId(void);"
    new_decl = "void* FindEntryByObjectId(BlendAnimationView* self, void* key);"
    old_signature = "void SetBlendEntryRecordFlag(void)"
    new_signature = "void SetBlendEntryRecordFlag(BlendAnimationView* self, void* key)"
    old_call = "(BlendSourceEntry*)FindEntryByObjectId();"
    new_call = "(BlendSourceEntry*)FindEntryByObjectId(self, key);"
    old_counts = (text.count(old_decl), text.count(old_signature), text.count(old_call))
    if old_counts == (1, 1, 1):
        text = text.replace(old_decl, new_decl, 1)
        text = text.replace(old_signature, new_signature, 1)
        return text.replace(old_call, new_call, 1)
    current = (
        "void* FindEntryByObjectId(BlendAnimationView* self, void* key);",
        "void SetBlendEntryRecordFlag(BlendAnimationView* blend, void* key)",
        "(BlendSourceEntry*)FindEntryByObjectId(blend, key);",
    )
    if tuple(text.count(item) for item in current) == (1, 1, 1):
        return text
    modern = (
        "void SetBlendEntryRecordFlag(BlendAnimationView* blend, void* key)",
        "(BlendSourceEntry*)FindEntryByObjectId(blend, (BaseObject*)key);",
    )
    if all(text.count(item) == 1 for item in modern):
        return text
    raise RuntimeError("expected blend entry lookup and callback ABI")


def canonicalize_hermite_vector_calls(text, label):
    """Route legacy EE-signature call sites through the matched host ABI."""
    # Recent decomp source already declares the helper with its host C
    # argument order (duration, four vectors, time/scale, outputs). Keep those
    # calls intact; the permutations below are for older EE-register-order
    # source only.
    if ("void EvaluateHermiteVectorSegment(f32 dur, const Vec02* p0, "
            "const Vec02* m0, const Vec02* p1, const Vec02* m1, f32 t, "
            "f32 k, Vec02* pos, Vec02* vel, Vec02* acc);" in text):
        return text
    # The current decomp emits cross-file prototypes into generated function
    # headers and includes that header from each source unit. Typed calls in
    # those units already use the declared semantic order.
    if re.search(r'(?m)^#include\s+"functions/curve_segment\.h"', text):
        return text
    order = {
        "main/effect.cpp": (7, 0, 1, 2, 3, 8, 9, 4, 5, 6),
        "main/path_curve.cpp": (0, 3, 4, 5, 6, 1, 2, 7, 8, 9),
        "main/movie_player.cpp": (0, 3, 4, 5, 6, 1, 2, 7, 8, 9),
        "main/world_listed_object.cpp": (4, 0, 1, 2, 3, 5, 6, 7, 8, 9),
        "main/agent.cpp": (7, 0, 1, 2, 3, 8, 9, 4, 5, 6),
        "main/player_state2.cpp": (0, 3, 4, 5, 6, 1, 2, 7, 9, None),
        "main/pose_source.cpp": (4, 0, 1, 2, 3, 5, 6, 7, 8, 9),
        "main/object_cycler.cpp": (0, 3, 4, 5, 6, 1, 2, 7, 8, 9),
        "main/player.cpp": (4, 0, 1, 2, 3, 5, 6, 7, None, None),
        "main/player_target.cpp": (0, 3, 4, 5, 6, 1, 2, 7, 8, 9),
    }
    indexes = order.get(label)
    if indexes is None:
        return text

    # The current decomp marks retail argument-register order with an explicit
    # function-pointer cast. Remove it in the host copy, then apply the same
    # argument mapping used for older direct calls.
    cast = re.compile(
        r"\(\(void \(\*\)\([^)]*\)\)EvaluateHermiteVectorSegment\)\(")
    text = cast.sub("EvaluateHermiteVectorSegment(", text)

    canonical = (
        "void EvaluateHermiteVectorSegment(f32 dur, void* p0, void* m0, "
        "void* p1, void* m1, f32 time, f32 scale, void* pos, void* vel, "
        "void* acc);"
    )
    text, declarations = re.subn(
        r"(?m)^void EvaluateHermiteVectorSegment\([^\n;]*\);$",
        canonical, text)
    if declarations < 1:
        raise RuntimeError("expected Hermite vector declaration in " + label)

    def split_args(args):
        parts = []
        start = 0
        depth = 0
        for index, char in enumerate(args):
            if char in "([":
                depth += 1
            elif char in ")]" and depth:
                depth -= 1
            elif char == "," and depth == 0:
                parts.append(args[start:index].strip())
                start = index + 1
        parts.append(args[start:].strip())
        return parts

    pattern = re.compile(r"EvaluateHermiteVectorSegment\(([^;\n]*)\);")
    calls = 0
    def reorder(match):
        nonlocal calls
        line_start = text.rfind("\n", 0, match.start()) + 1
        if text[line_start:match.start()].strip().startswith("void"):
            return match.group(0)
        args = split_args(match.group(1))
        if len(args) != len(indexes):
            raise RuntimeError("unexpected Hermite vector argument count in " + label)
        calls += 1
        return "EvaluateHermiteVectorSegment(" + ", ".join(
            args[index] if index is not None else "0" for index in indexes) + ");"
    text = pattern.sub(reorder, text)
    if calls == 0:
        raise RuntimeError("expected Hermite vector call in " + label)
    return text


def forward_keyed_rotation_get_arguments(text, label):
    """Forward the rotation-get virtual arguments to the keyed evaluator."""
    if label != "main/pose_source.cpp":
        return text
    current_signature = (
        "void GetKeyedRotationCurve(KeyedRotationCurve* self, PlacedObject* other, "
        "s32 flags, f32* values, f32* slopes, f32 key, f32 scale)")
    if current_signature in text and "EvaluateVectorKeyCurve(self->count, self->keys, flags, values, slopes, key, scale);" in text:
        return text
    old_signature = "void GetKeyedRotationCurve(KeyedRotationCurve* self)"
    new_signature = (
        "void GetKeyedRotationCurve(KeyedRotationCurve* self, PlacedObject* owner, "
        "s32 mode, void* outAngles, void* outVec, f32 key, f32 scale)")
    if text.count(old_signature) != 1:
        raise RuntimeError("expected keyed rotation getter")
    text = text.replace(old_signature, new_signature, 1)
    old_call = "EvaluateVectorKeyCurve(self->count, self->keys);"
    new_call = (
        "EvaluateVectorKeyCurve(self->count, (s32)self->keys, mode, "
        "(s32)outAngles, (s32)outVec, key, scale);")
    if text.count(old_call) != 1:
        raise RuntimeError("expected keyed rotation evaluation")
    return text.replace(old_call, new_call, 1)


def refresh_attachment_target_matrix_context(text, label):
    """Reload the context after lookup before building its target matrix."""
    if label != "main/placed_object_model.cpp":
        return text
    # The current decomp has a typed matrix return and keeps the matrix
    # operation in a single expression. The older host32 workaround below
    # rewrites its untyped output-pointer ABI, so it does not apply here.
    if ("QuadMatrix MultiplyMatrices(const QuadMatrix& a, const QuadMatrix& b);" in text and
            "e->m = MultiplyMatrices(b, a);" in text):
        return text
    if ("Mat4A MultiplyMatrices(const Mat4A& a, const Mat4A& b);" in text and
            "*(Mat4A*)&e->m = MultiplyMatrices(*(Mat4A*)&b, *(Mat4A*)&a);" in text):
        return text
    declarations = (
        'extern "C" {\n'
        'extern volatile unsigned int sly1_native_bind_self;\n'
        'extern volatile unsigned int sly1_native_bind_context;\n'
        'extern volatile unsigned int sly1_native_bind_world;\n'
        'extern volatile unsigned int sly1_native_bind_key;\n'
        'extern volatile unsigned int sly1_native_find_world;\n'
        'extern volatile unsigned int sly1_native_find_id;\n'
        'extern volatile unsigned int sly1_native_find_context;\n'
        'extern volatile unsigned int sly1_native_find_esp_before;\n'
        'extern volatile unsigned int sly1_native_find_esp_after;\n'
        'extern volatile unsigned int sly1_native_matrix_source;\n'
        'extern volatile unsigned int sly1_native_matrix_output;\n'
        'extern volatile unsigned int sly1_native_bind_ebp_entry;\n'
        'extern volatile unsigned int sly1_native_bind_ebp_after_find;\n'
        '}\n\n')
    text = declarations + text
    function = "void BindAttachmentTargets(AttachmentTargets* self, PlacedObject* ctx)\n{\n"
    instrumented = (
        function +
        "    sly1_native_bind_self = (unsigned int)self;\n"
        "    sly1_native_bind_context = (unsigned int)ctx;\n"
        "    sly1_native_bind_world = (unsigned int)ctx->world;\n"
        "    __asm__ volatile(\"movl %%ebp, %0\" : \"=m\"(sly1_native_bind_ebp_entry));\n")
    if text.count(function) != 1:
        raise RuntimeError("expected BindAttachmentTargets definition")
    text = text.replace(function, instrumented, 1)
    lookup = (
        "        TargetObject* r = (TargetObject*)FindFirstObjectByIdMode4("
        "ctx->world, self->keys[i], ctx);")
    lookup_replacement = (
        "        sly1_native_bind_key = (unsigned int)self->keys[i];\n"
        "        sly1_native_find_world = (unsigned int)ctx->world;\n"
        "        sly1_native_find_id = (unsigned int)self->keys[i];\n"
        "        sly1_native_find_context = (unsigned int)ctx;\n"
        "        __asm__ volatile(\"movl %%esp, %0\" : \"=m\"(sly1_native_find_esp_before));\n"
        "        TargetObject* r = (TargetObject*)FindFirstObjectByIdMode4("
        "ctx->world, self->keys[i], ctx);\n"
        "        __asm__ volatile(\"movl %%esp, %0\" : \"=m\"(sly1_native_find_esp_after));\n"
        "        __asm__ volatile(\"movl %%ebp, %0\" : \"=m\"(sly1_native_bind_ebp_after_find));\n"
        "        sly1_native_matrix_source = (unsigned int)&ctx->m110;\n"
        "        sly1_native_matrix_output = (unsigned int)&f.a;")
    if text.count(lookup) != 1:
        raise RuntimeError("expected attachment target lookup in " + label)
    text = text.replace(lookup, lookup_replacement, 1)
    old_decl = "void MultiplyMatrices(void* out, void* b, void* in); // stays void: retail returns a value, but declared so func_00165828 differs in registers"
    new_decl = "QuadMatrix MultiplyMatrices(Quad16* b, Quad16* in);"
    if text.count(old_decl) != 1:
        raise RuntimeError("expected retail attachment matrix prototype in " + label)
    text = text.replace(old_decl, new_decl, 1)
    old_call = "            MultiplyMatrices(&f.m, &f.b, &f.a);"
    new_call = "            f.m = MultiplyMatrices(&f.b, &f.a);"
    if text.count(old_call) != 1:
        raise RuntimeError("expected attachment matrix multiply in " + label)
    return text.replace(old_call, new_call, 1)


def preserve_postload_attachment_context(text, label):
    """Keep the host object pointer across the attachment callback boundary."""
    if label != "main/placed_object.cpp":
        return text
    declarations = (
        'extern "C" {\n'
        'extern volatile unsigned int sly1_native_postload_entry;\n'
        'extern volatile unsigned int sly1_native_postload_bind;\n'
        '}\n\n')
    text = declarations + text
    function = "void PostLoadPlacedObject(PlacedObject* self)\n{\n"
    if text.count(function) != 1:
        raise RuntimeError("expected PostLoadPlacedObject definition in " + label)
    text = text.replace(
        function,
        function +
        "    PlacedObject* volatile attachment_context = self;\n"
        "    sly1_native_postload_entry = (unsigned int)self;\n",
        1)
    call = "    BindAttachmentTargets(self->sub22C, self);"
    replacement = (
        "    sly1_native_postload_bind = (unsigned int)attachment_context;\n"
        "    BindAttachmentTargets(attachment_context->sub22C, attachment_context);")
    latest_call = "    BindAttachmentTargets((AttachmentTargets*)self->sub22C, self);"
    latest_replacement = (
        "    sly1_native_postload_bind = (unsigned int)attachment_context;\n"
        "    BindAttachmentTargets((AttachmentTargets*)attachment_context->sub22C, "
        "attachment_context);")
    if text.count(call) == 1:
        return text.replace(call, replacement, 1)
    if text.count(latest_call) == 1:
        return text.replace(latest_call, latest_replacement, 1)
    else:
        raise RuntimeError("expected PostLoadPlacedObject attachment call in " + label)


def supply_player_collider_position(text, label):
    """Pass the player's current position to its two-argument collider update."""
    if label != "main/player.cpp":
        return text
    if ("void SetColliderPosition(Collider* self, Vec02* pos);" in text and
            "SetColliderPosition(self, pos);" in text):
        return text
    if (re.search(r'(?m)^#include\s+"functions/collider\.h"', text) and
            "SetColliderPosition(self, pos);" in text):
        return text
    old_decl = "void SetColliderPosition(void* self);"
    new_decl = "void SetColliderPosition(void* self, void* pos);"
    if text.count(old_decl) != 1:
        raise RuntimeError("expected player collider position prototype")
    text = text.replace(old_decl, new_decl, 1)
    old_call = "    SetColliderPosition(self);"
    new_call = "    SetColliderPosition(self, (u8*)self + 0x100);"
    if text.count(old_call) != 1:
        raise RuntimeError("expected player collider position call")
    return text.replace(old_call, new_call, 1)


def normalize_turning_collider_matrix_abi(text, label):
    """Use the host matrix function's axis/output/angle C++ call order."""
    if label != "main/turning_collider.cpp":
        return text
    if ("void BuildAxisAngleMatrix(const Vec02* v, Mat02* out, f32 angle);" in text and
            "BuildAxisAngleMatrix(&g_unitZVector, (Mat02*)((u8*)self + 0x660), self->heading);" in text):
        return text
    if (re.search(r'(?m)^#include\s+"functions/matrix\.h"', text) and
            "BuildAxisAngleMatrix(&g_unitZVector, &self->headingMatrix, self->heading);" in text and
            re.search(r"BuildAxisAngleMatrix\(&g_unitZVector, \((?:Mat02|Mat3)\*\)matrix, angle\);", text)):
        return text
    old_decl = "void BuildAxisAngleMatrix(const Vec02* t, f32 x, void* p);"
    new_decl = "void BuildAxisAngleMatrix(const Vec02* axis, void* out, f32 angle);"
    if text.count(old_decl) != 1:
        raise RuntimeError("expected turning collider matrix prototype")
    text = text.replace(old_decl, new_decl, 1)
    replacements = (
        ("BuildAxisAngleMatrix(&g_unitZVector, self->f638, (u8*)self + 0x660);",
         "BuildAxisAngleMatrix(&g_unitZVector, (u8*)self + 0x660, self->f638);"),
        ("BuildAxisAngleMatrix(&g_unitZVector, angle, matrix);",
         "BuildAxisAngleMatrix(&g_unitZVector, matrix, angle);"),
    )
    for old, new in replacements:
        if text.count(old) != 1:
            raise RuntimeError("expected turning collider matrix call: " + old)
        text = text.replace(old, new, 1)
    return text


def normalize_gif_register_writer_abi(text):
    """Give every staged AddGifRegisterWrite declaration the host ABI."""
    signature = re.compile(
        r"(?m)(void\s+AddGifRegisterWrite\s*\(\s*"
        r"(?:PacketWriter|void)\s*\*\s*\w+\s*,\s*)"
        r"(?:s32|u64)\s+(\w+)\s*,\s*"
        r"(?:s32|s64|u64)\s+(\w+)(\s*\))")
    return signature.sub(r"\1s32 \2, u64 \3\4", text)


def normalize_agent_axis_angle_matrix_abi(text, label):
    """Match Agent's local axis-angle prototype to the compiled host definition."""
    if label != "main/agent.cpp":
        return text
    old_decl = "void BuildAxisAngleMatrix(const Vec4* v, f32 angle, Mat3* out);"
    new_decl = "void BuildAxisAngleMatrix(const Vec4* v, Mat3* out, f32 angle);"
    retail_call = (
        "((void (*)(const Vec4*, f32, Mat3*))BuildAxisAngleMatrix)"
        "((const Vec4*)&g_unitZVector, x, &self->headingMatrix);")
    host_call = (
        "BuildAxisAngleMatrix((const Vec4*)&g_unitZVector, "
        "&self->headingMatrix, x);")
    host_spin_call = (
        "BuildAxisAngleMatrix((const Vec4*)&g_unitZVector, &spin, self->heading);")
    if old_decl not in text and text.count(retail_call) == 1 and text.count(host_spin_call) == 1:
        return text.replace(retail_call, host_call, 1)
    if text.count(old_decl) != 1:
        raise RuntimeError("expected agent axis-angle matrix prototype")
    text = text.replace(old_decl, new_decl, 1)
    replacements = (
        ("BuildAxisAngleMatrix((const Vec4*)&g_unitZVector, x, &self->headingMatrix);",
         "BuildAxisAngleMatrix((const Vec4*)&g_unitZVector, &self->headingMatrix, x);"),
        ("BuildAxisAngleMatrix((const Vec4*)&g_unitZVector, self->heading, &spin);",
         "BuildAxisAngleMatrix((const Vec4*)&g_unitZVector, &spin, self->heading);"),
    )
    for old, new in replacements:
        if text.count(old) != 1:
            raise RuntimeError("expected agent axis-angle matrix call: " + old)
        text = text.replace(old, new, 1)
    return text


def normalize_link_constraint_axis_angle_matrix_abi(text, label):
    """Match link constraints' local axis-angle prototype to the host definition."""
    if label != "main/link_constraint.cpp":
        return text
    old_decl = "void BuildAxisAngleMatrix(f32 angle, const Vec4* v, Mat3* out);"
    new_decl = "void BuildAxisAngleMatrix(const Vec4* v, Mat3* out, f32 angle);"
    retail_call = (
        "((void (*)(f32, const Vec4*, Mat3*))BuildAxisAngleMatrix)"
        "(Arccosine(c) - angle, &axis, &bend);")
    host_call = "BuildAxisAngleMatrix(&axis, &bend, Arccosine(c) - angle);"
    host_twist_call = (
        "BuildAxisAngleMatrix(&dir, &twist, self->linkRecord->angle);")
    if old_decl not in text and text.count(retail_call) == 1 and text.count(host_twist_call) == 1:
        return text.replace(retail_call, host_call, 1)
    if text.count(old_decl) != 1:
        raise RuntimeError("expected link-constraint axis-angle matrix prototype")
    text = text.replace(old_decl, new_decl, 1)
    replacements = (
        ("BuildAxisAngleMatrix(Arccosine(c) - angle, &axis, &bend);",
         "BuildAxisAngleMatrix(&axis, &bend, Arccosine(c) - angle);"),
        ("BuildAxisAngleMatrix(self->linkRecord->angle, &dir, &twist);",
         "BuildAxisAngleMatrix(&dir, &twist, self->linkRecord->angle);"),
    )
    for old, new in replacements:
        if text.count(old) != 1:
            raise RuntimeError("expected link-constraint axis-angle call: " + old)
        text = text.replace(old, new, 1)
    return text


def make_axis_curve_outputs_unaligned_safe(text, label):
    """Copy axis-curve vector outputs without assuming Vec4 alignment."""
    if label != "main/pose_source.cpp":
        return text
    old_values = "        *(Vec4*)outValues = *base;"
    new_values = "        __builtin_memcpy(outValues, base, sizeof(*base));"
    old_slopes = "        Vu0::Q(*(Vec4*)outSlopes) = Vu0::Zero();"
    new_slopes = "        __builtin_memset(outSlopes, 0, sizeof(Vec4));"
    for old, new, description in (
        (old_values, new_values, "axis-curve value initialization"),
        (old_slopes, new_slopes, "axis-curve slope initialization"),
    ):
        if text.count(new) == 1:
            continue
        if text.count(old) != 1:
            raise RuntimeError("expected " + description)
        text = text.replace(old, new, 1)
    return text


def normalize_attachment_matrix_value_abi(text, label):
    """Pass attachment matrices by reference in the host ABI.

    The retail MIPS sources use a by-value aggregate in these declarations.
    On i386, that makes the caller/callee disagree about the hidden return
    pointer and the location of the matrix arguments. The host matrix routine
    already accepts references, so normalize only the staged declarations.
    """
    if label not in ("main/placed_object_model.cpp", "main/game_progress.cpp"):
        return text
    if (label == "main/placed_object_model.cpp" and
            "Mat4A MultiplyMatrices(const Mat4A& a, const Mat4A& b);" in text and
            "*(Mat4A*)&e->m = MultiplyMatrices(" in text):
        return text
    old = "QuadMatrix MultiplyMatrices(QuadMatrix a, void* q);"
    new = "QuadMatrix MultiplyMatrices(const QuadMatrix& a, const QuadMatrix& q);"
    if label == "main/placed_object_model.cpp":
        calls = (
            ("e = MultiplyMatrices(b, &c);", "e = MultiplyMatrices(b, c);"),
            ("e = MultiplyMatrices(a, &c);", "e = MultiplyMatrices(a, c);"),
            ("el->m50 = MultiplyMatrices(e, &el->q10);",
             "el->m50 = MultiplyMatrices(e, *(const QuadMatrix*)&el->q10);"),
        )
        for source, replacement in calls:
            if text.count(replacement) == 1:
                continue
            if text.count(source) != 1:
                raise RuntimeError("expected attachment matrix call: " + source)
            text = text.replace(source, replacement, 1)
    if old not in text:
        if label == "main/placed_object_model.cpp" and text.count(new) >= 2:
            return text
        if label == "main/game_progress.cpp" and new in text:
            return text
        raise RuntimeError("expected by-value attachment matrix declaration in " + label)
    return text.replace(old, new)


def normalize_colored_sprite_packet_abi(text, label):
    """Use the host C++ parameter order at retail MIPS sprite call sites."""
    declaration = (
        "void WriteColoredSpritePacket(s32 r, s32 g, s32 b, s32 a, "
        "PacketWriter* p, f32 x0, f32 y0, f32 x1, f32 y1);"
    )
    host_declaration = (
        "void WriteColoredSpritePacket(f32 x0, f32 y0, f32 x1, f32 y1, "
        "s32 r, s32 g, s32 b, s32 a, PacketWriter* p);"
    )
    if label not in ("main/warp_menu.cpp", "main/pause_menu.cpp"):
        return text
    current_calls = {
        "main/warp_menu.cpp": (
            ("((void (*)(s32, s32, s32, s32, PacketWriter*, f32, f32, f32, f32))"
             "WriteColoredSpritePacket)(0, 0, 0, (s32)v, &g_gsPacketWriter, "
             "0.0f, 0.0f, 640.0f, 492.80002f);",
             "WriteColoredSpritePacket(0.0f, 0.0f, 640.0f, 492.80002f, "
             "0, 0, 0, (s32)v, &g_gsPacketWriter);"),
        ),
        "main/pause_menu.cpp": (
            ("((void (*)(s32, s32, s32, s32, PacketWriter*, f32, f32, f32, f32))"
             "WriteColoredSpritePacket)(r, g, b, 255, &buf.f, 10.0f, y, z, "
             "TruncateFloat(412.909088f) * 1.1f + 6.6000004f);",
             "WriteColoredSpritePacket(10.0f, y, z, "
             "TruncateFloat(412.909088f) * 1.1f + 6.6000004f, "
             "r, g, b, 255, &buf.f);"),
            ("((void (*)(s32, s32, s32, s32, PacketWriter*, f32, f32, f32, f32))"
             "WriteColoredSpritePacket)(r, g, b, 255, &buf.f, x, y, z, "
             "TruncateFloat(412.909088f) * 1.1f + 6.6000004f);",
             "WriteColoredSpritePacket(x, y, z, "
             "TruncateFloat(412.909088f) * 1.1f + 6.6000004f, "
             "r, g, b, 255, &buf.f);"),
        ),
    }[label]
    if any(text.count(old) for old, _ in current_calls):
        for old, new in current_calls:
            if old in text:
                text = text.replace(old, new, 1)
        return text
    if declaration not in text:
        if host_declaration in text:
            return text
        raise RuntimeError("expected retail-order sprite writer declaration in " + label)
    text = text.replace(declaration, host_declaration, 1)
    calls = {
        "main/warp_menu.cpp": (
            ("WriteColoredSpritePacket(0, 0, 0, (s32)v, &g_gsPacketWriter, "
             "0.0f, 0.0f, 640.0f, 492.80002f);",
             "WriteColoredSpritePacket(0.0f, 0.0f, 640.0f, 492.80002f, "
             "0, 0, 0, (s32)v, &g_gsPacketWriter);"),
        ),
        "main/pause_menu.cpp": (
            ("WriteColoredSpritePacket(r, g, b, 255, &buf.f, 10.0f, y, z, "
             "TruncateFloat(412.909088f) * 1.1f + 6.6000004f);",
             "WriteColoredSpritePacket(10.0f, y, z, "
             "TruncateFloat(412.909088f) * 1.1f + 6.6000004f, "
             "r, g, b, 255, &buf.f);"),
            ("WriteColoredSpritePacket(r, g, b, 255, &buf.f, x, y, z, "
             "TruncateFloat(412.909088f) * 1.1f + 6.6000004f);",
             "WriteColoredSpritePacket(x, y, z, "
             "TruncateFloat(412.909088f) * 1.1f + 6.6000004f, "
             "r, g, b, 255, &buf.f);"),
        ),
    }[label]
    for old, new in calls:
        if new in text:
            continue
        if old not in text:
            raise RuntimeError("expected sprite writer call in " + label)
        text = text.replace(old, new, 1)
    return text


def normalize_placed_object_sound_abi(text, label):
    """Reorder retail's MIPS-oriented sound call declarations for host calls."""
    if label != "main/placed_object.cpp":
        return text
    old_decls = (
        "void StartSoundEffect(s32 a0, f32 f12, f32 f13, f32 f14, f32 f15, "
        "f32 f16, void* a1, PlacedObject* a2, s32 a3, void* a4, s32 a5);",
        "void StartSoundEffect(void* a0, f32 f12, f32 f13, f32 f14, f32 f15, "
        "f32 f16, void* a1, void* a2, s32 a3, void* a4, s32 a5);",
    )
    if (sum(text.count(old) for old in old_decls) == 0 and
            text.count("void StartSoundEffect(s32 id, PlayingSound** out, ") >= 1):
        return text
    new_decl = (
        "struct PlayingSound;\n"
        "void StartSoundEffect(s32 id, PlayingSound** out, s32 p2, s32 p3, "
        "f32 farRange, f32 nearRange, f32 baseVolume, f32 pitch, f32 f16, "
        "f32* interval, s32 p5);"
    )
    for old in old_decls:
        if text.count(old) != 1:
            raise RuntimeError("expected MIPS-oriented placed-object sound prototype")
        text = text.replace(old, new_decl, 1)

    calls = (
        ("StartSoundEffect(q->f00, q->f04, q->f08, q->f0C, q->f10, q->f20, "
         "&q->f1C, self, 0, &q->b14, 0);",
         "StartSoundEffect(q->f00, (PlayingSound**)&q->f1C, 0, (s32)self, q->f04, q->f08, "
         "q->f0C, q->f10, q->f20, (f32*)&q->b14, 0);"),
        ("StartSoundEffect(p->p00, p->f04, p->f08, p->f0C, p->f10, p->f20, "
         "&p->p1C, self, 0, &p->b14, 0);",
         "StartSoundEffect((s32)p->p00, (PlayingSound**)&p->p1C, 0, (s32)self, p->f04, p->f08, "
         "p->f0C, p->f10, p->f20, (f32*)&p->b14, 0);"),
    )
    for old, new in calls:
        if text.count(old) != 2 and old.startswith("StartSoundEffect(p->"):
            raise RuntimeError("expected two placed-object sound-record calls")
        if old.startswith("StartSoundEffect(q->") and text.count(old) != 1:
            raise RuntimeError("expected placed-object initial sound call")
        text = text.replace(old, new)
    return text


def normalize_script_host_call_abi(text, label):
    """Translate retail-address host callbacks before a direct native call."""
    if label != "main/script_machine.cpp":
        return text
    call = "        host->fn(&t, host->self, count, arr);"
    if text.count(call) != 1:
        raise RuntimeError("expected script host-function dispatch")
    text = text.replace(
        call,
        "        unsigned int native_host_fn = "
        "sly1_native_resolve_function((unsigned int)host->fn);\n"
        "        if (native_host_fn != 0)\n"
        "            host->fn = reinterpret_cast<decltype(host->fn)>(native_host_fn);\n"
        + call,
        1)
    return ('extern "C" unsigned int sly1_native_resolve_function(unsigned int);\n' + text)


def make_point_frame_transform_unaligned_safe(text, label):
    """Keep 16-byte vector copies safe for host callers with Vec3 temporaries."""
    if label != "main/placed_object.cpp":
        return text
    old = (
        "void TransformPointBetweenFrames(PlacedObject* from, PlacedObject* to, Vec02* in, Vec02* out)\n"
        "{\n"
        "    Vec02 world;\n"
        "    Vec02 local;\n"
        "    if (from != to) {\n"
        "        if (from != 0) {\n"
        "            Vu0::Q(world) = Vu0::Rotate(from->m110, Vu0::Q(*in));\n"
        "            Vu0::Q(world) = Vu0::Add(Vu0::Q(from->v140), Vu0::Q(world));\n"
        "            in = &world;\n"
        "        }\n"
        "        if (to != 0) {\n"
        "            Vu0::Q(local) = Vu0::Sub(Vu0::Q(*in), Vu0::Q(to->v140));\n"
        "            Vu0::Q(*out) = Vu0::RotateTransposed(to->m110, Vu0::Q(local));\n"
        "            return;\n"
        "        }\n"
        "    }\n"
        "    *out = *in;\n"
        "}\n"
    )
    new = (
        "void TransformPointBetweenFrames(PlacedObject* from, PlacedObject* to, Vec02* in, Vec02* out)\n"
        "{\n"
        "    Vec02 input;\n"
        "    Vec02 world;\n"
        "    Vec02 local;\n"
        "    __builtin_memcpy(&input, in, sizeof(input));\n"
        "    if (from != to) {\n"
        "        if (from != 0) {\n"
        "            Vu0::Q(world) = Vu0::Rotate(from->m110, Vu0::Q(input));\n"
        "            Vu0::Q(world) = Vu0::Add(Vu0::Q(from->v140), Vu0::Q(world));\n"
        "            input = world;\n"
        "        }\n"
        "        if (to != 0) {\n"
        "            Vu0::Q(local) = Vu0::Sub(Vu0::Q(input), Vu0::Q(to->v140));\n"
        "            Vu0::Q(local) = Vu0::RotateTransposed(to->m110, Vu0::Q(local));\n"
        "            __builtin_memcpy(out, &local, sizeof(local));\n"
        "            return;\n"
        "        }\n"
        "    }\n"
        "    __builtin_memcpy(out, &input, sizeof(input));\n"
        "}\n"
    )
    if text.count(old) == 1:
        pass
    elif text.count(old.replace("Vec02", "Vec4")) == 1:
        old = old.replace("Vec02", "Vec4")
        new = new.replace("Vec02", "Vec4")
    else:
        raise RuntimeError("expected point-frame transform definition")
    return text.replace(old, new, 1)


def make_rotation_translation_matrix_unaligned_safe(text, label):
    """Read possibly unaligned object matrices without host aligned SIMD loads."""
    if label != "main/matrix.cpp":
        return text
    signature = re.compile(
        r"void BuildMatrixFromRotationAndTranslation\(const (?:Vec02|Vec4)\* pos, "
        r"const (?:Mat02|Mat3)\* rot, Mat4A\* out\)\n\{\n"
    )
    new_body = """    const Quad16* a = (const Quad16*)pos;
    const Quad16* b = (const Quad16*)rot;
    Quad16* dst = (Quad16*)out;
    dst[0] = b[0];
    dst[1] = b[1];
    dst[2] = b[2];
    dst[3] = *a;
    dst[0].w[3] = 0;
    dst[1].w[3] = 0;
    dst[2].w[3] = 0;
    *(f32*)&dst[3].w[3] = 1.0f;
}
"""
    aligned_body = """    volatile const u8* source = (volatile const u8*)rot;
    u8* destination = (u8*)out;
    for (s32 i = 0; i < 0x30; ++i) destination[i] = source[i];
    source = (volatile const u8*)pos;
    destination += 0x30;
    for (s32 i = 0; i < 0x10; ++i) destination[i] = source[i];
    Quad16* dst = (Quad16*)out;
    dst[0].w[3] = 0;
    dst[1].w[3] = 0;
    dst[2].w[3] = 0;
    *(f32*)&dst[3].w[3] = 1.0f;
}
"""
    matches = list(signature.finditer(text))
    if len(matches) != 1 or not text.startswith(new_body, matches[0].end()):
        raise RuntimeError("expected rotation/translation matrix definition")
    body_start = matches[0].end()
    body_end = body_start + len(new_body)
    return text[:body_start] + aligned_body + text[body_end:]


def align_collider_position_argument(text, label):
    """Copy an EE quadword input to aligned host storage before SIMD reads."""
    if label != "main/collider.cpp":
        return text
    pattern = re.compile(
        r"void SetColliderPosition\(Collider\* self, (Vec02|Vec4)\* pos\)\n\{\n"
    )
    matches = list(pattern.finditer(text))
    if len(matches) != 1:
        raise RuntimeError("expected SetColliderPosition definition in " + label)
    vector_type = matches[0].group(1)
    signature = matches[0].group(0)
    replacement = (signature + f"    {vector_type} aligned_pos;\n"
                   "    __builtin_memcpy(&aligned_pos, pos, sizeof(aligned_pos));\n"
                   "    pos = &aligned_pos;\n")
    return text[:matches[0].start()] + replacement + text[matches[0].end():]


def make_null_memory_copy_deterministic(text, label):
    """Model the cleared EE address-zero page for native host memory copies."""
    if label != "main/heap.cpp":
        return text
    signatures = (
        "void CopyMemory(u8* dst, u8* src, u32 size)\n{\n",
        "void CopyMemory(void* dst, void* src, u32 size)\n{\n",
    )
    signature = next((item for item in signatures if text.count(item) == 1), None)
    if signature is None or sum(text.count(item) for item in signatures) != 1:
        raise RuntimeError("expected CopyMemory definition in " + label)
    replacement = (
        signature
        + "    if (src == 0) {\n"
        + "        __builtin_memset(dst, 0, size);\n"
        + "        return;\n"
        + "    }\n"
    )
    return text.replace(signature, replacement, 1)


def map_null_turning_collider_face_reads(text, label):
    """Use a zeroed host buffer where EE code reads its address-zero RAM."""
    if label != "main/turning_collider.cpp":
        return text
    needle = (
        "                    MeshFace* f = &((MeshFace*)self->mesh.faces)[j];")
    replacement = (
        "                    static MeshFace host_zero_page[2048];\n"
        "                    MeshFace* face_base = (MeshFace*)self->mesh.faces;\n"
        "                    if (face_base == 0) face_base = host_zero_page;\n"
        "                    MeshFace* f = &face_base[j];")
    if text.count(needle) != 1:
        raise RuntimeError("expected turning-collider face read in " + label)
    return text.replace(needle, replacement, 1)


def rebuild_collider_mesh_motion_hook(text, label):
    """Drop the old host hook before rebuilding it from newly copied meshes."""
    if label != "main/collider.cpp":
        return text
    call = "    InitColliderMeshMotion(self);"
    replacement = "    self->hook = 0;\n" + call
    if text.count(call) != 1:
        raise RuntimeError("expected collider mesh-motion initialization in " + label)
    return text.replace(call, replacement, 1)


def trace_collider_mesh_motion_allocations(text, label):
    """Capture mesh-motion allocation results for the next native fault dump."""
    if label != "main/collider.cpp":
        return text
    declarations = (
        'extern "C" {\n'
        'extern volatile unsigned int sly1_native_motion_stage;\n'
        'extern volatile unsigned int sly1_native_motion_self;\n'
        'extern volatile unsigned int sly1_native_motion_hook;\n'
        'extern volatile unsigned int sly1_native_motion_velocity;\n'
        'extern volatile unsigned int sly1_native_motion_accel;\n'
        'extern volatile unsigned int sly1_native_motion_tris;\n'
        'extern volatile unsigned int sly1_native_motion_entry_hook;\n'
        'extern volatile unsigned int sly1_native_motion_entry_velocity;\n'
        'extern volatile unsigned int sly1_native_motion_entry_accel;\n'
        'extern volatile unsigned int sly1_native_motion_entry_tris;\n'
        'extern volatile unsigned int sly1_native_motion_entry_other;\n'
        'extern volatile unsigned int sly1_native_motion_entry_self_id;\n'
        'extern volatile unsigned int sly1_native_motion_entry_other_id;\n'
        'extern volatile unsigned int sly1_native_motion_entry_self_vertices;\n'
        'extern volatile unsigned int sly1_native_motion_entry_other_vertices;\n'
        'extern volatile unsigned int sly1_native_motion_entry_self_desc;\n'
        'extern volatile unsigned int sly1_native_motion_entry_other_desc;\n'
        'extern volatile unsigned int sly1_native_motion_helper_src;\n'
        'extern volatile unsigned int sly1_native_motion_helper_extra;\n'
        'extern volatile unsigned int sly1_native_motion_helper_src_id;\n'
        'extern volatile unsigned int sly1_native_motion_helper_src_vertices;\n'
        'extern volatile unsigned int sly1_native_motion_entry_all;\n'
        'extern volatile unsigned int sly1_native_motion_entry_self_faces;\n'
        'extern volatile unsigned int sly1_native_motion_entry_self_face_count;\n'
        'extern volatile unsigned int sly1_native_motion_entry_self_vert_ptr;\n'
        'extern volatile unsigned int sly1_native_motion_entry_self_copy_count;\n'
        'extern volatile unsigned int sly1_native_motion_entry_other_faces;\n'
        'extern volatile unsigned int sly1_native_motion_entry_other_face_count;\n'
        'extern volatile unsigned int sly1_native_motion_entry_return;\n'
        'extern volatile unsigned int sly1_native_motion_copy_calls;\n'
        'extern volatile unsigned int sly1_native_motion_copy_self;\n'
        'extern volatile unsigned int sly1_native_motion_copy_vertices;\n'
        '}\n\n')
    text = declarations + text
    copy_signature = "void CopyColliderMeshesInFrame(Collider* self, Collider* src, s32 extra)\n{\n"
    if text.count(copy_signature) != 1:
        raise RuntimeError("expected CopyColliderMeshesInFrame definition")
    text = text.replace(
        copy_signature,
        copy_signature
        + "    sly1_native_motion_helper_src = (unsigned int)src;\n"
        + "    sly1_native_motion_helper_extra = (unsigned int)extra;\n"
        + "    sly1_native_motion_helper_src_id = (unsigned int)src->id;\n"
        + "    sly1_native_motion_helper_src_vertices = (unsigned int)src->mesh.vertCount;\n",
        1)
    signature = "void InitColliderMeshMotion(Collider* self)\n{\n"
    if text.count(signature) != 1:
        raise RuntimeError("expected InitColliderMeshMotion definition")
    text = text.replace(
        signature,
        signature
        + "    sly1_native_motion_stage = 1;\n"
        + "    sly1_native_motion_self = (unsigned int)self;\n",
        1)
    updates = (
        ("    self->hook = h;\n",
         "    self->hook = h;\n"
         "    sly1_native_motion_hook = (unsigned int)h;\n"
         "    sly1_native_motion_stage = 2;\n"),
        ("    h->tris = (MeshMotion::Tri*)AllocateZeroedFrontMemory(self->meshCopy.faceCount * 12);\n",
         "    h->tris = (MeshMotion::Tri*)AllocateZeroedFrontMemory(self->meshCopy.faceCount * 12);\n"
         "    sly1_native_motion_tris = (unsigned int)h->tris;\n"
         "    sly1_native_motion_stage = 5;\n"),
    )
    for old, new in updates:
        if text.count(old) != 1:
            raise RuntimeError("expected mesh-motion allocation assignment")
        text = text.replace(old, new, 1)
    for field, trace_name, stage in (("velocity", "velocity", 3),
                                     ("accel", "accel", 4)):
        pattern = re.compile(
            rf"(?m)^(    h->{field} = \((?:Vec02|Vec4)\*\)"
            rf"AllocateZeroedFrontMemory\(self->meshCopy\.vertCount \* 16\);)$"
        )
        text, replacements = pattern.subn(
            rf"\1\n    sly1_native_motion_{trace_name} = (unsigned int)h->{field};"
            rf"\n    sly1_native_motion_stage = {stage};",
            text)
        if replacements != 1:
            raise RuntimeError("expected mesh-motion vector allocation assignment")
    return text


def trace_turning_mesh_hook_use(text, label):
    """Record hook fields at the transfer call and immediately before motion."""
    if label != "main/turning_collider.cpp":
        return text
    declarations = (
        'extern "C" {\n'
        'extern volatile unsigned int sly1_native_motion_stage;\n'
        'extern volatile unsigned int sly1_native_motion_self;\n'
        'extern volatile unsigned int sly1_native_motion_hook;\n'
        'extern volatile unsigned int sly1_native_motion_velocity;\n'
        'extern volatile unsigned int sly1_native_motion_accel;\n'
        'extern volatile unsigned int sly1_native_motion_tris;\n'
        'extern volatile unsigned int sly1_native_motion_entry_hook;\n'
        'extern volatile unsigned int sly1_native_motion_entry_velocity;\n'
        'extern volatile unsigned int sly1_native_motion_entry_accel;\n'
        'extern volatile unsigned int sly1_native_motion_entry_tris;\n'
        'extern volatile unsigned int sly1_native_motion_entry_other;\n'
        'extern volatile unsigned int sly1_native_motion_entry_self_id;\n'
        'extern volatile unsigned int sly1_native_motion_entry_other_id;\n'
        'extern volatile unsigned int sly1_native_motion_entry_self_vertices;\n'
        'extern volatile unsigned int sly1_native_motion_entry_other_vertices;\n'
        'extern volatile unsigned int sly1_native_motion_entry_self_desc;\n'
        'extern volatile unsigned int sly1_native_motion_entry_other_desc;\n'
        'extern volatile unsigned int sly1_native_motion_entry_all;\n'
        'extern volatile unsigned int sly1_native_motion_entry_self_faces;\n'
        'extern volatile unsigned int sly1_native_motion_entry_self_face_count;\n'
        'extern volatile unsigned int sly1_native_motion_entry_self_vert_ptr;\n'
        'extern volatile unsigned int sly1_native_motion_entry_self_copy_count;\n'
        'extern volatile unsigned int sly1_native_motion_entry_other_faces;\n'
        'extern volatile unsigned int sly1_native_motion_entry_other_face_count;\n'
        'extern volatile unsigned int sly1_native_motion_entry_return;\n'
        'extern volatile unsigned int sly1_native_motion_copy_calls;\n'
        'extern volatile unsigned int sly1_native_motion_copy_self;\n'
        'extern volatile unsigned int sly1_native_motion_copy_vertices;\n'
        '}\n\n')
    text = declarations + text
    signature = "void SetTurningColliderFollowedMesh(TurningCollider* self, Collider* other, s32 all)\n{\n"
    entry_probe = (
        signature
        + "    sly1_native_motion_stage = 6;\n"
        + "    sly1_native_motion_self = (unsigned int)self;\n"
        + "    sly1_native_motion_hook = (unsigned int)self->hook;\n"
        + "    sly1_native_motion_entry_other = (unsigned int)other;\n"
        + "    sly1_native_motion_entry_self_id = (unsigned int)self->id;\n"
        + "    sly1_native_motion_entry_other_id = (unsigned int)other->id;\n"
        + "    sly1_native_motion_entry_self_vertices = (unsigned int)self->mesh.vertCount;\n"
        + "    sly1_native_motion_entry_other_vertices = (unsigned int)other->mesh.vertCount;\n"
        + "    sly1_native_motion_entry_self_desc = (unsigned int)self->desc;\n"
        + "    sly1_native_motion_entry_other_desc = (unsigned int)other->desc;\n"
        + "    sly1_native_motion_entry_all = (unsigned int)all;\n"
        + "    sly1_native_motion_entry_self_faces = (unsigned int)self->mesh.faces;\n"
        + "    sly1_native_motion_entry_self_face_count = (unsigned int)self->mesh.faceCount;\n"
        + "    sly1_native_motion_entry_self_vert_ptr = (unsigned int)self->mesh.verts;\n"
        + "    sly1_native_motion_entry_self_copy_count = (unsigned int)self->meshCopy.vertCount;\n"
        + "    sly1_native_motion_entry_other_faces = (unsigned int)other->mesh.faces;\n"
        + "    sly1_native_motion_entry_other_face_count = (unsigned int)other->mesh.faceCount;\n"
        + "    sly1_native_motion_entry_return = (unsigned int)__builtin_return_address(0);\n"
        + "    if (self->hook != 0) {\n"
        + "        unsigned int* motion = (unsigned int*)self->hook;\n"
        + "        sly1_native_motion_entry_hook = (unsigned int)self->hook;\n"
        + "        sly1_native_motion_entry_velocity = motion[0];\n"
        + "        sly1_native_motion_entry_accel = motion[1];\n"
        + "        sly1_native_motion_entry_tris = motion[2];\n"
        + "    }\n")
    if text.count(signature) != 1:
        raise RuntimeError("expected SetTurningColliderFollowedMesh definition")
    text = text.replace(signature, entry_probe, 1)
    call = "            AdvanceColliderMeshMotion(self, 0.0f);"
    pre_call = (
        "            sly1_native_motion_stage = 7;\n"
        "            sly1_native_motion_hook = (unsigned int)self->hook;\n"
        "            if (self->hook != 0) {\n"
        "                unsigned int* motion = (unsigned int*)self->hook;\n"
        "                sly1_native_motion_velocity = motion[0];\n"
        "                sly1_native_motion_accel = motion[1];\n"
        "                sly1_native_motion_tris = motion[2];\n"
        "            }\n"
        + call)
    if text.count(call) != 1:
        raise RuntimeError("expected turning-collider mesh-motion call")
    return text.replace(call, pre_call, 1)


def trace_turning_mesh_copy_state(text, label):
    """Preserve the turning-collider copy hook's hidden MIPS ABI arguments on x86."""
    if label != "main/turning_collider.cpp":
        return text
    current_signature = (
        "void CopyTurningColliderMeshesInFrame(TurningCollider* self, "
        "Collider* other, s32 mode)\n{\n")
    if current_signature in text:
        declarations = (
            'extern "C" {\n'
            'extern volatile unsigned int sly1_native_motion_copy_calls;\n'
            'extern volatile unsigned int sly1_native_motion_copy_self;\n'
            'extern volatile unsigned int sly1_native_motion_copy_vertices;\n'
            'extern volatile unsigned int sly1_native_motion_copy_entry_return;\n'
            'extern volatile unsigned int sly1_native_motion_copy_entry_arg1;\n'
            'extern volatile unsigned int sly1_native_motion_copy_entry_arg2;\n'
            '}\n\n')
        probe = (current_signature
                 + "    sly1_native_motion_copy_calls++;\n"
                 + "    sly1_native_motion_copy_self = (unsigned int)self;\n"
                 + "    sly1_native_motion_copy_vertices = (unsigned int)self->mesh.vertCount;\n"
                 + "    unsigned int* copy_frame = (unsigned int*)__builtin_frame_address(0);\n"
                 + "    sly1_native_motion_copy_entry_return = (unsigned int)copy_frame[1];\n"
                 + "    sly1_native_motion_copy_entry_arg1 = (unsigned int)other;\n"
                 + "    sly1_native_motion_copy_entry_arg2 = (unsigned int)mode;\n")
        if text.count(current_signature) != 1:
            raise RuntimeError("expected current turning-collider mesh-copy definition")
        return (declarations + text).replace(current_signature, probe, 1)
    declarations = (
        'extern "C" {\n'
        'extern volatile unsigned int sly1_native_motion_copy_calls;\n'
        'extern volatile unsigned int sly1_native_motion_copy_self;\n'
        'extern volatile unsigned int sly1_native_motion_copy_vertices;\n'
        'extern volatile unsigned int sly1_native_motion_copy_entry_return;\n'
        'extern volatile unsigned int sly1_native_motion_copy_entry_arg1;\n'
        'extern volatile unsigned int sly1_native_motion_copy_entry_arg2;\n'
        '}\n\n')
    text = declarations + text
    copy_declaration = "void CopyColliderMeshesInFrame(void* self);"
    if text.count(copy_declaration) != 1:
        raise RuntimeError("expected one-argument collider mesh-copy declaration")
    text = text.replace(
        copy_declaration,
        "void CopyColliderMeshesInFrame(Collider* self, Collider* src, s32 extra);",
        1)
    signature = "void CopyTurningColliderMeshesInFrame(TurningCollider* self)\n{\n"
    adapted_signature = (
        "void CopyTurningColliderMeshesInFrame(TurningCollider* self, Collider* src, s32 extra)\n{\n")
    replacement = (
        adapted_signature
        + "    sly1_native_motion_copy_calls++;\n"
        + "    sly1_native_motion_copy_self = (unsigned int)self;\n"
        + "    sly1_native_motion_copy_vertices = (unsigned int)self->mesh.vertCount;\n"
        + "    unsigned int* copy_frame = (unsigned int*)__builtin_frame_address(0);\n"
        + "    sly1_native_motion_copy_entry_return = (unsigned int)copy_frame[1];\n"
        + "    sly1_native_motion_copy_entry_arg1 = (unsigned int)src;\n"
        + "    sly1_native_motion_copy_entry_arg2 = (unsigned int)extra;\n")
    if text.count(signature) != 1:
        raise RuntimeError("expected CopyTurningColliderMeshesInFrame definition")
    text = text.replace(signature, replacement, 1)
    call = "    CopyColliderMeshesInFrame(self);"
    if text.count(call) != 1:
        raise RuntimeError("expected one-argument turning-collider mesh-copy call")
    return text.replace(call, "    CopyColliderMeshesInFrame(self, src, extra);", 1)


def lower_gcc_minmax(text):
    """Lower the old GCC ``>?`` maximum operator in staged C++ units."""
    compound_used = False
    text, count = re.subn(
        r"(?P<lhs>\*\(f32\*\)&self->[A-Za-z_]\w*)\s*>\?=\s*(?P<rhs>[^;]+);",
        lambda m: m.group("lhs") + " = sly1_host_max(" + m.group("lhs") +
        ", " + m.group("rhs") + ");", text)
    compound_used |= count != 0
    text, count = re.subn(
        r"(?<!->)\b(?P<lhs>[A-Za-z_]\w*)\s*>\?=\s*(?P<rhs>[A-Za-z_]\w*)",
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


def return_appended_call_record(text, label):
    """Complete the matched append helper's return in the staged host copy."""
    if label != "main/world.cpp":
        return text
    signature = "QueuedCall* AppendQueuedCall(World* self, void* code, void* arg, s32 c, s32 d)"
    start = -1
    search = 0
    body_start = -1
    while True:
        candidate = text.find(signature, search)
        if candidate < 0:
            break
        brace = text.find("{", candidate + len(signature))
        if brace >= 0 and not text[candidate + len(signature):brace].strip():
            start = candidate
            body_start = brace
            break
        search = candidate + len(signature)
    if start < 0:
        raise RuntimeError("expected AppendQueuedCall definition in " + label)
    body_end = text.find("\n}", body_start)
    if body_start < 0 or body_end < 0:
        raise RuntimeError("expected AppendQueuedCall body in " + label)
    body = text[body_start:body_end]
    if "return e;" in body:
        return text
    if "self->queuedCallTail = e;" not in body:
        raise RuntimeError("expected AppendQueuedCall tail update in " + label)
    # The matched body falls off a non-void function. That is undefined in C++;
    # GCC 16 optimized away the tail update on the empty-queue path. The return
    # value is the record just appended, and this host-only completion preserves
    # the source body while making its existing queue update well-defined.
    return text[:body_end] + "\n    return e;" + text[body_end:]


def trace_arrival_lookup(text, label):
    """Capture the exact world/lookup/object values before arrival transforms."""
    if label != "main/oscillator.cpp":
        return text
    declarations = (
        'extern "C" {\n'
        'extern volatile unsigned int sly1_native_arrival_world;\n'
        'extern volatile unsigned int sly1_native_arrival_id;\n'
        'extern volatile unsigned int sly1_native_arrival_parent;\n'
        'extern volatile unsigned int sly1_native_arrival_result;\n'
        'extern volatile unsigned int sly1_native_arrival_place_self;\n'
        'extern volatile unsigned int sly1_native_arrival_rotation_self;\n'
        '}\n\n')
    text = declarations + text
    replacements = [
        ("HoldingMember* FindArrivalPoint(s32 id, s32 parent)\n{\n",
         "HoldingMember* FindArrivalPoint(s32 id, s32 parent)\n{\n"
         "    sly1_native_arrival_world = (unsigned int)g_currentWorld;\n"
         "    sly1_native_arrival_id = (unsigned int)id;\n"
         "    sly1_native_arrival_parent = (unsigned int)parent;\n"),
        ("void PlaceAtArrivalPoint(TransformSet* self)\n{\n",
         "void PlaceAtArrivalPoint(TransformSet* self)\n{\n"
         "    sly1_native_arrival_place_self = (unsigned int)self;\n"),
        ("void GetObjectWorldRotation(BaseObject* self, Mat02* out)\n{\n",
         "void GetObjectWorldRotation(BaseObject* self, Mat02* out)\n{\n"
         "    sly1_native_arrival_rotation_self = (unsigned int)self;\n")]
    for old, new in replacements:
        if text.count(old) == 1:
            text = text.replace(old, new, 1)
            continue
        if old.startswith("void GetObjectWorldRotation("):
            signature = re.compile(
                r"void GetObjectWorldRotation\(BaseObject\* self, "
                r"(?:Mat02|Mat3)\* out\)\n\{\n"
            )
            matches = list(signature.finditer(text))
            if len(matches) == 1:
                addition = new[len(old):]
                text = (text[:matches[0].start()] + matches[0].group(0) +
                        addition + text[matches[0].end():])
                continue
        raise RuntimeError("expected arrival trace site in " + label)
    function_start = "HoldingMember* FindArrivalPoint(s32 id, s32 parent)\n{\n"
    if text.count(function_start) != 1:
        raise RuntimeError("expected arrival lookup definition in " + label)
    start = text.index(function_start)
    end = text.index("\n}\n", start) + 2
    function = text[start:end]
    return_statement = "    return found;"
    if function.count(return_statement) != 1:
        raise RuntimeError("expected arrival lookup return in " + label)
    function = function.replace(
        return_statement,
        "    sly1_native_arrival_result = (unsigned int)found;\n" + return_statement,
        1)
    text = text[:start] + function + text[end:]
    return text


def return_from_world_transform_wrappers(text, label):
    """Give non-returning decomp declarations a host return instruction."""
    if label != "main/oscillator.cpp":
        return text
    replacements = [
        ("    TransformPointBetweenFrames(self->parent, 0, (u8*)self + 0x40, arg);\n}",
         "    TransformPointBetweenFrames(self->parent, 0, (u8*)self + 0x40, arg);\n"
         "    return 0;\n}"),
        ("    TransformRotationBetweenFrames(self->parent, 0, (u8*)self + 0x50, arg);\n}",
         "    TransformRotationBetweenFrames(self->parent, 0, (u8*)self + 0x50, arg);\n"
         "    return 0;\n}")]
    for old, new in replacements:
        count = text.count(old)
        if count > 1:
            raise RuntimeError("expected world transform wrapper in " + label)
        if count == 1:
            text = text.replace(old, new, 1)
    return text


def normalize_player_probe_call_abi(text, label):
    """Pass probe outputs and radius in the matched helper's C argument order."""
    if label != "main/player.cpp":
        return text
    modern_calls = (
        ("((void (*)(void*, PlacedObject*, void*, PlacedObject*, f32, FollowProbe*))"
         "BuildPartProbeTowardLink)(self, (PlacedObject*)a1, a1, "
         "(PlacedObject*)self->f620, k, (FollowProbe*)((u8*)&self->footProbeA));",
         "BuildPartProbeTowardLink(self, (PlacedObject*)a1, a1, "
         "(PlacedObject*)self->f620, (FollowProbe*)((u8*)&self->footProbeA), k);"),
        ("((void (*)(void*, PlacedObject*, void*, PlacedObject*, f32, FollowProbe*))"
         "BuildPartProbeTowardLink)(self, (PlacedObject*)self->linkPartB, "
         "self->linkPartB, (PlacedObject*)self->f624, k, "
         "(FollowProbe*)((u8*)&self->footProbeB));",
         "BuildPartProbeTowardLink(self, (PlacedObject*)self->linkPartB, "
         "self->linkPartB, (PlacedObject*)self->f624, "
         "(FollowProbe*)((u8*)&self->footProbeB), k);"),
    )
    if all(text.count(old) == 1 for old, _ in modern_calls):
        for old, new in modern_calls:
            text = text.replace(old, new, 1)
        return text
    current_decl = (
        "void BuildPartProbeTowardLink(void* unused0, PlacedObject* self, "
        "void* unused2, PlacedObject* obj, f32 radius, FollowProbe* out);")
    current_native_decl = (
        "void BuildPartProbeTowardLink(void* unused0, PlacedObject* self, "
        "void* unused2, PlacedObject* obj, FollowProbe* out, f32 radius);")
    if text.count(current_decl) == 1:
        text = text.replace(current_decl, current_native_decl, 1)
        typed_calls = [
            ("BuildPartProbeTowardLink(self, (PlacedObject*)a1, a1, "
             "(PlacedObject*)self->f620, k, (FollowProbe*)((u8*)&self->f1190));",
             "BuildPartProbeTowardLink(self, (PlacedObject*)a1, a1, "
             "(PlacedObject*)self->f620, (FollowProbe*)((u8*)&self->f1190), k);"),
            ("BuildPartProbeTowardLink(self, (PlacedObject*)self->f614, self->f614, "
             "(PlacedObject*)self->f624, k, (FollowProbe*)((u8*)&self->f1290));",
             "BuildPartProbeTowardLink(self, (PlacedObject*)self->f614, self->f614, "
             "(PlacedObject*)self->f624, (FollowProbe*)((u8*)&self->f1290), k);")]
        if all(text.count(old) == 1 for old, _ in typed_calls):
            for old, new in typed_calls:
                text = text.replace(old, new, 1)
            return text
        replacements = [
            ("BuildPartProbeTowardLink(self, (PlacedObject*)a1, a1, "
             "(PlacedObject*)self->f620, k, (FollowProbe*)((u8*)&self->footProbeA));",
             "BuildPartProbeTowardLink(self, (PlacedObject*)a1, a1, "
             "(PlacedObject*)self->f620, (FollowProbe*)((u8*)&self->footProbeA), k);"),
            ("BuildPartProbeTowardLink(self, (PlacedObject*)self->linkPartB, self->linkPartB, "
             "(PlacedObject*)self->f624, k, (FollowProbe*)((u8*)&self->footProbeB));",
             "BuildPartProbeTowardLink(self, (PlacedObject*)self->linkPartB, self->linkPartB, "
             "(PlacedObject*)self->f624, (FollowProbe*)((u8*)&self->footProbeB), k);"),
            ("BuildPartProbeTowardLink(self, (PlacedObject*)a1, a1, "
             "(PlacedObject*)self->f620, k, (FollowProbe*)((u8*)self + 0x1190));",
             "BuildPartProbeTowardLink(self, (PlacedObject*)a1, a1, "
             "(PlacedObject*)self->f620, (FollowProbe*)((u8*)self + 0x1190), k);"),
            ("BuildPartProbeTowardLink(self, (PlacedObject*)self->f614, self->f614, "
             "(PlacedObject*)self->f624, k, (FollowProbe*)((u8*)self + 0x1290));",
             "BuildPartProbeTowardLink(self, (PlacedObject*)self->f614, self->f614, "
             "(PlacedObject*)self->f624, (FollowProbe*)((u8*)self + 0x1290), k);")]
        applied = 0
        for old, new in replacements:
            count = text.count(old)
            if count == 1:
                text = text.replace(old, new, 1)
                applied += 1
            elif count > 1:
                raise RuntimeError("ambiguous current player probe call")
        if applied != 2:
            raise RuntimeError("expected two current player probe calls")
        return text
    old_decl = "void BuildPartProbeTowardLink(Player* self, void* a1, void* a2, s32 a3, f32 f12, void* a4);"
    new_decl = "void BuildPartProbeTowardLink(s32 a0, void* self, s32 a2, void* obj, void* out, f32 radius);"
    if text.count(old_decl) != 1:
        raise RuntimeError("expected player probe declaration")
    text = text.replace(old_decl, new_decl, 1)
    replacements = [
        ("BuildPartProbeTowardLink(self, a1, a1, (s32)self->f620, k, (u8*)self + 0x1190);",
         "BuildPartProbeTowardLink(0, a1, 0, a1, (u8*)self + 0x1190, k);"),
        ("BuildPartProbeTowardLink(self, self->f614, self->f614, (s32)self->f624, k, (u8*)self + 0x1290);",
         "BuildPartProbeTowardLink(0, self->f614, 0, self->f614, (u8*)self + 0x1290, k);")]
    for old, new in replacements:
        if text.count(old) != 1:
            raise RuntimeError("expected player probe call")
        text = text.replace(old, new, 1)
    return text


def forward_ground_contact_arguments(text, label):
    """Forward the PS2 caller's live contact/index arguments on native x86."""
    if label != "main/player.cpp":
        return text
    if ("s32 IsTurningColliderGroundContact(TurningCollider* self, Contact03* a, s32 idx);" in text and
            "return IsTurningColliderGroundContact(self, e, idx);" in text):
        return text
    if (re.search(r'(?m)^#include\s+"functions/turning_collider\.h"', text) and
            "return IsTurningColliderGroundContact(self, e, idx);" in text):
        return text
    old_decl = "s32 IsTurningColliderGroundContact(void* self);"
    new_decl = "s32 IsTurningColliderGroundContact(void* self, Contact03* contact, s32 idx);"
    if text.count(old_decl) != 1:
        raise RuntimeError("expected turning-collider contact declaration")
    text = text.replace(old_decl, new_decl, 1)
    old_call = "        return IsTurningColliderGroundContact(self);"
    new_call = "        return IsTurningColliderGroundContact(self, e, idx);"
    if text.count(old_call) != 1:
        raise RuntimeError("expected turning-collider contact call")
    return text.replace(old_call, new_call, 1)


def forward_player_slide_contact_index(text, label):
    """Carry the contact side index across the player's nested vtable call."""
    if label != "main/player.cpp":
        return text
    old_typedef = "typedef s32 (*VFn_170410)(Player* self, void* out);"
    new_typedef = "typedef s32 (*VFn_170410)(Player* self, void* out, s32 idx);"
    if text.count(old_typedef) != 1:
        raise RuntimeError("expected player slide callback typedef")
    text = text.replace(old_typedef, new_typedef, 1)
    signature = re.compile(
        r"void ComputePlayerSlideRates\(Player\* self, (Contact03|BodyContact)\* out\)"
    )
    matches = list(signature.finditer(text))
    if len(matches) != 1:
        raise RuntimeError("expected player slide-rate definition")
    old_signature = matches[0].group(0)
    new_signature = old_signature[:-1] + ", s32 idx)"
    text = text[:matches[0].start()] + new_signature + text[matches[0].end():]
    old_call = "((VFn_170410*)self->desc)[90](self, out)"
    new_call = "((VFn_170410*)self->desc)[90](self, out, idx)"
    if text.count(old_call) != 1:
        raise RuntimeError("expected player slide callback call")
    return text.replace(old_call, new_call, 1)


def trace_contact_notification_callback(text, label):
    """Record the exact contact end object and slot-63 callback target."""
    if label != "main/collider_contact.cpp":
        return text
    declarations = (
        'extern "C" {\n'
        'extern volatile unsigned int sly1_native_contact_object;\n'
        'extern volatile unsigned int sly1_native_contact_descriptor;\n'
        'extern volatile unsigned int sly1_native_contact_callback;\n'
        'extern volatile unsigned int sly1_native_contact_end;\n'
        '}\n\n')
    text = declarations + text
    old = re.compile(
        r"(?m)^(\s*)\(\(void \(\*\)\(Collider\*, \w+\*, s32\)\)"
        r"c->end\[e\]\.part->desc03\(\)->m3B\)"
        r"\(c->end\[e\]\.part, c, e\);$"
    )
    matches = list(old.finditer(text))
    if len(matches) != 1:
        raise RuntimeError("expected contact notification callback")
    indent = matches[0].group(1)
    callback_type = re.search(r"Collider\*, (\w+\*), s32", matches[0].group(0)).group(1)
    new = (
        f"{indent}sly1_native_contact_object = (unsigned int)c->end[e].part;\n"
        f"{indent}sly1_native_contact_descriptor = (unsigned int)c->end[e].part->desc03();\n"
        f"{indent}sly1_native_contact_callback = (unsigned int)c->end[e].part->desc03()->m3B;\n"
        f"{indent}sly1_native_contact_end = (unsigned int)e;\n"
        f"{indent}((void (*)(Collider*, {callback_type}, s32))"
        "c->end[e].part->desc03()->m3B)(c->end[e].part, c, e);")
    return text[:matches[0].start()] + new + text[matches[0].end():]


def trace_camera_target_callback(text, label):
    """Capture a camera target's slot-73 function before invoking it."""
    if label != "main/camera.cpp":
        return text
    declarations = (
        'extern "C" {\n'
        'extern volatile unsigned int sly1_native_camera_target;\n'
        'extern volatile unsigned int sly1_native_camera_target_descriptor;\n'
        'extern volatile unsigned int sly1_native_camera_target_callback;\n'
        '}\n\n')
    text = declarations + text
    old = (
        "        ((VFn_143418)(*(void***)o)[0x124 / 4])(o, out, dt);")
    new = (
        "        sly1_native_camera_target = (unsigned int)o;\n"
        "        sly1_native_camera_target_descriptor = (unsigned int)*(void**)o;\n"
        "        sly1_native_camera_target_callback = (unsigned int)(*(void***)o)[0x124 / 4];\n"
        "        ((VFn_143418)(*(void***)o)[0x124 / 4])(o, out, dt);")
    if text.count(old) != 1:
        raise RuntimeError("expected camera target callback")
    return text.replace(old, new, 1)


def forward_switched_pose_arguments(text, label, legacy_ramp_abi=True):
    """Restore the PS2 argument registers consumed by the nested pose call."""
    if label != "main/switched_collider.cpp":
        return text
    prototype = "void CaptureColliderPose(void);"
    call = "    CaptureColliderPose();"
    if text.count(prototype) == 1 and text.count(call) == 1:
        text = text.replace(prototype, "void CaptureColliderPose(void* self, void* out);", 1)
        text = text.replace(call, "    CaptureColliderPose(self, out);", 1)
    elif not (("void CaptureColliderPose(Collider* self, ColliderPose* out);" in text and
               "CaptureColliderPose(self, (ColliderPose*)out);" in text) or
              (re.search(r'(?m)^#include\s+"functions/collider\.h"', text) and
               "CaptureColliderPose(self, (ColliderPose*)out);" in text)):
        raise RuntimeError("expected switched-collider pose call")
    if not legacy_ramp_abi:
        return text
    ramp_decl = "f32 ApproachValueWithRamp(f32 a, f32 b, f32 c, Quad16* p, s32 zero);"
    ramp_call = "ApproachValueWithRamp(self->v560.z, self->v140.z, a, (Quad16*)D_00269BC0, 0)"
    current_ramp_decl = (
        "f32 ApproachValueWithRamp(f32 x, f32 y, f32 z, RampProfile* k, f32* out);")
    current_ramp_call = (
        "ApproachValueWithRamp(self->focusPoint.z, self->v140.z, a, "
        "(RampProfile*)g_followZRamp, 0)")
    if text.count(current_ramp_decl) == 1 and text.count(current_ramp_call) == 1:
        text = text.replace(current_ramp_decl,
                            "f32 ApproachValueWithRamp(RampProfile* k, f32* out, f32 x, f32 y, f32 z);", 1)
        return text.replace(
            current_ramp_call,
            "ApproachValueWithRamp((RampProfile*)g_followZRamp, 0, "
            "self->focusPoint.z, self->v140.z, a)", 1)
    if text.count(ramp_decl) != 1 or text.count(ramp_call) != 1:
        raise RuntimeError("expected switched-collider ramp call")
    text = text.replace(
        ramp_decl,
        "f32 ApproachValueWithRamp(Quad16* p, f32* out, f32 a, f32 b, f32 c);",
        1)
    return text.replace(
        ramp_call,
        "ApproachValueWithRamp((Quad16*)D_00269BC0, 0, self->v560.z, self->v140.z, a)",
        1)


def normalize_update_camera_target_pose_abi(text, label):
    """Normalize the PS2 f12/output order for a target pose callback on x86."""
    if label != "main/camera.cpp":
        return text
    old_type = "typedef void (*VFn_143E40)(void* self, f32 dt, void* a);"
    new_type = "typedef void (*VFn_143E40)(void* self, void* out, f32 dt);"
    old_call = "((VFn_143E40)(*(void***)o)[0x124 / 4])(o, dt, x);"
    new_call = "((VFn_143E40)(*(void***)o)[0x124 / 4])(o, x, dt);"
    old_tracking_decl = "void UpdateCameraTargetTracking(Camera* c, void* n, f32 dt, void* x);"
    new_tracking_decl = "void UpdateCameraTargetTracking(Camera* c, void* n, void* x, f32 dt);"
    old_tracking_call = "UpdateCameraTargetTracking(self, o, dt, x);"
    new_tracking_call = "UpdateCameraTargetTracking(self, o, x, dt);"
    latest_tracking_decl = (
        "void UpdateCameraTargetTracking(Spring_00143208* self, void* target, "
        "f32 dt, Record_00143208* rec);"
    )
    latest_tracking_call = (
        "UpdateCameraTargetTracking((Spring_00143208*)self, o, dt, "
        "(Record_00143208*)x);"
    )
    latest_tracking_decl_new = (
        "void UpdateCameraTargetTracking(Spring_00143208* self, void* target, "
        "Record_00143208* rec, f32 dt);"
    )
    latest_tracking_call_new = (
        "UpdateCameraTargetTracking((Spring_00143208*)self, o, "
        "(Record_00143208*)x, dt);"
    )
    if text.count(old_type) != 1 or text.count(old_call) != 1:
        raise RuntimeError("expected UpdateCamera target-pose call")
    text = text.replace(old_type, new_type, 1).replace(old_call, new_call, 1)
    current_tracking_definition = (
        "void UpdateCameraTargetTracking(Spring_00143208* self, void* target, "
        "f32 dt, Record_00143208* rec)\n{")
    if (text.count(current_tracking_definition) == 1 and
            text.count(latest_tracking_call) == 1):
        # The current decomp defines this function in the same unit with the
        # same typed order as its caller. Keep that pair consistent on host.
        return text
    if text.count(old_tracking_decl) == 1 and text.count(old_tracking_call) == 1:
        return text.replace(old_tracking_decl, new_tracking_decl, 1).replace(
            old_tracking_call, new_tracking_call, 1)
    if text.count(latest_tracking_decl) == 1 and text.count(latest_tracking_call) == 1:
        return text.replace(latest_tracking_decl, latest_tracking_decl_new, 1).replace(
            latest_tracking_call, latest_tracking_call_new, 1)
    raise RuntimeError("expected UpdateCamera target-pose call")


def normalize_on_off_camera_ramp_abi(text, label, legacy_ramp_abi=True):
    """Match the shared ramp helper's host signature in the camera controller."""
    if label != "main/on_off_object.cpp":
        return text
    if not legacy_ramp_abi:
        return text
    old_decl = "f32 ApproachValueWithRamp(f32 value, f32 target, f32 dt, RampProfile* profile, f32* speed);"
    new_decl = "f32 ApproachValueWithRamp(RampProfile* profile, f32* speed, f32 value, f32 target, f32 dt);"
    old_call = "ApproachValueWithRamp(view->f44, (*(s32*)&eye->q[0] & 1) ? 0.0f : block->f64, dt, &D_00275A80, 0)"
    new_call = "ApproachValueWithRamp(&D_00275A80, 0, view->f44, (*(s32*)&eye->q[0] & 1) ? 0.0f : block->f64, dt)"
    latest_decl = "f32 ApproachValueWithRamp(f32 x, f32 y, f32 z, RampProfile* k, f32* out);"
    latest_call = (
        "ApproachValueWithRamp(view->turnScale, "
        "(*(s32*)&eye->q[0] & 1) ? 0.0f : block->turnRate, dt, "
        "&g_onOffCameraFadeRamp, 0)"
    )
    if text.count(latest_decl) == 1 and text.count(latest_call) == 1:
        latest_new_decl = (
            "f32 ApproachValueWithRamp(RampProfile* k, f32* out, "
            "f32 x, f32 y, f32 z);")
        latest_new_call = (
            "ApproachValueWithRamp(&g_onOffCameraFadeRamp, 0, view->turnScale, "
            "(*(s32*)&eye->q[0] & 1) ? 0.0f : block->turnRate, dt)")
        return text.replace(latest_decl, latest_new_decl, 1).replace(
            latest_call, latest_new_call, 1)
    if text.count(old_decl) == 1 and text.count(old_call) == 1:
        return text.replace(old_decl, new_decl, 1).replace(old_call, new_call, 1)
    raise RuntimeError("expected on-off camera ramp call")


def normalize_turning_collider_ramp_abi(text, label, legacy_ramp_abi=True):
    """Normalize this unit's PS2 mixed-register ramp calls for the host ABI."""
    if label != "main/turning_collider.cpp":
        return text
    if not legacy_ramp_abi:
        return text
    old_decl = "f32 ApproachValueWithRamp(f32 x, f32 y, f32 z, RampProfile* k, f32* out);"
    new_decl = "f32 ApproachValueWithRamp(RampProfile* k, f32* out, f32 x, f32 y, f32 z);"
    hook_calls = (
        (
            "ApproachValueWithRamp((&cur->x)[k], (&world.x)[k], "
            "g_gameClock.dt, (RampProfile*)D_00274B68, 0)",
            "ApproachValueWithRamp((RampProfile*)D_00274B68, 0, "
            "(&cur->x)[k], (&world.x)[k], g_gameClock.dt)",
        ),
        (
            "ApproachValueWithRamp((&cur->x)[k], (&world.x)[k], "
            "g_gameClock.dt, (RampProfile*)g_turningColliderHookRamp, 0)",
            "ApproachValueWithRamp((RampProfile*)g_turningColliderHookRamp, 0, "
            "(&cur->x)[k], (&world.x)[k], g_gameClock.dt)",
        ),
    )
    calls = (
        (
            "ApproachValueWithRamp(cur, 0.0f, g_gameClock.dt, "
            "(RampProfile*)&A, &out1)",
            "ApproachValueWithRamp((RampProfile*)&A, &out1, cur, "
            "0.0f, g_gameClock.dt)",
        ),
        (
            "ApproachValueWithRamp(cur, -self->probeDepth, g_gameClock.dt, "
            "(RampProfile*)&B, &out2)",
            "ApproachValueWithRamp((RampProfile*)&B, &out2, cur, "
            "-self->probeDepth, g_gameClock.dt)",
        ),
    )
    if (text.count(old_decl) != 1 or
            sum(text.count(old) for old, _ in hook_calls) != 1 or
            any(text.count(old) != 1 for old, _ in calls)):
        raise RuntimeError("expected turning-collider ramp declaration and calls")
    text = text.replace(old_decl, new_decl, 1)
    for old, new in (*hook_calls, *calls):
        text = text.replace(old, new, 1)
    return text


def normalize_pose_source_ramp_abi(text, label, legacy_ramp_abi=True):
    """Canonicalize float-first ramp calls in the pose-source host copy."""
    if label != "main/pose_source.cpp":
        return text
    if not legacy_ramp_abi:
        return text
    old_decl = "extern f32 ApproachValueWithRamp(f32 x, f32 y, f32 z, RampProfile* k, f32* out);"
    new_decl = "extern f32 ApproachValueWithRamp(RampProfile* k, f32* out, f32 x, f32 y, f32 z);"
    calls = (
        (
            "ApproachValueWithRamp(0.0f, dist, dt, &prof, &speed)",
            "ApproachValueWithRamp(&prof, &speed, 0.0f, dist, dt)",
        ),
        (
            "ApproachValueWithRamp(owner->linkRecord->angle, value, dt, "
            "(RampProfile*)owner->p220, &owner->linkRecord->angleSlope)",
            "ApproachValueWithRamp((RampProfile*)owner->p220, "
            "&owner->linkRecord->angleSlope, owner->linkRecord->angle, value, dt)",
        ),
        (
            "ApproachValueWithRamp(0.0f, angle, dt, (RampProfile*)&prof, &rate)",
            "ApproachValueWithRamp((RampProfile*)&prof, &rate, 0.0f, angle, dt)",
        ),
    )
    if text.count(old_decl) != 1 or any(text.count(old) != (2 if "owner->linkRecord" in old else 1)
                                        for old, _ in calls):
        raise RuntimeError("expected pose-source float-first ramp declarations and calls")
    text = text.replace(old_decl, new_decl, 1)
    for old, new in calls:
        text = text.replace(old, new, 2 if "owner->linkRecord" in old else 1)
    return text


def normalize_optional_owner_position_outputs(text, label):
    """Preserve null optional output pointers in the native host copy."""
    if label != "main/pose_source.cpp":
        return text
    old = '''void GetOwnerPosition(Attachment* self, Vec02* out1, Vec02* out2)
{
    *out1 = self->owner->v190;
    *out2 = *(Vec02*)D_00248D30;
    if (self->owner->desc02()->m28 != 0) {
        ((PositionOutToldFn)self->owner->desc02()->m28)(self->owner, out1);
    }
}'''
    new = '''void GetOwnerPosition(Attachment* self, Vec02* out1, Vec02* out2)
{
    Vec02 fallback;
    Vec02* position = out1 != 0 ? out1 : &fallback;
    *position = self->owner->v190;
    if (out2 != 0) {
        *out2 = *(Vec02*)D_00248D30;
    }
    if (self->owner->desc02()->m28 != 0) {
        ((PositionOutToldFn)self->owner->desc02()->m28)(self->owner, position);
    }
}'''
    if text.count(old) == 1:
        pass
    elif text.count(old.replace("Vec02", "Vec4").replace(
            "*(Vec4*)D_00248D30", "*(Vec4*)g_zeroVector")) == 1:
        old = old.replace("Vec02", "Vec4").replace(
            "*(Vec4*)D_00248D30", "*(Vec4*)g_zeroVector")
        new = new.replace("Vec02", "Vec4").replace(
            "*(Vec4*)D_00248D30", "*(Vec4*)g_zeroVector")
    else:
        raise RuntimeError("expected GetOwnerPosition source body")
    text = text.replace(old, new, 1)
    old_rotation = '''void GetOwnerRotation(Attachment* self, Mat02* out1, Vec02* out2)
{
    *out1 = self->owner->m1A0;
    *out2 = *(Vec02*)D_00248D30;
    ((FrameOutToldFn)self->owner->desc02()->m29)(self->owner, out1, out2);
}'''
    new_rotation = '''void GetOwnerRotation(Attachment* self, Mat02* out1, Vec02* out2)
{
    Mat02 fallback_rotation;
    Vec02 fallback_vector;
    Mat02* rotation = out1 != 0 ? out1 : &fallback_rotation;
    Vec02* vector = out2 != 0 ? out2 : &fallback_vector;
    *rotation = self->owner->m1A0;
    *vector = *(Vec02*)D_00248D30;
    ((FrameOutToldFn)self->owner->desc02()->m29)(self->owner, rotation, vector);
}'''
    if text.count(old_rotation) == 0:
        old_rotation = old_rotation.replace("Mat02", "Mat3").replace(
            "Vec02", "Vec4").replace("*(Vec4*)D_00248D30", "*(Vec4*)g_zeroVector")
        new_rotation = new_rotation.replace("Mat02", "Mat3").replace(
            "Vec02", "Vec4").replace("*(Vec4*)D_00248D30", "*(Vec4*)g_zeroVector")
    if text.count(old_rotation) != 1:
        raise RuntimeError("expected GetOwnerRotation source body")
    return text.replace(old_rotation, new_rotation, 1)


def normalize_optional_child_rotation_outputs(text, label):
    """Supply temporary result storage for omitted child-rotation outputs."""
    if label != "main/player_aim.cpp":
        return text
    signature = re.compile(
        r"void CombineChildPoseRotations\(ChildPoseSource\* self, "
        r"(Mat02|Mat3)\* outRot, (Vec02|Vec4)\* outVec, f32 t\)\n\{"
    )
    matches = list(signature.finditer(text))
    if len(matches) != 1:
        raise RuntimeError("expected CombineChildPoseRotations definition")
    match = matches[0]
    start = match.end()
    depth = 1
    end = start
    while end < len(text) and depth:
        if text[end] == "{":
            depth += 1
        elif text[end] == "}":
            depth -= 1
        end += 1
    if depth:
        raise RuntimeError("unterminated CombineChildPoseRotations body")
    body = text[start:end - 1]
    body = body.replace("*outRot", "*rotation").replace("outRot,", "rotation,")
    body = body.replace("*outVec", "*vector").replace("outVec,", "vector,")
    body = (f"\n    {match.group(1)} fallbackRotation;\n"
            f"    {match.group(2)} fallbackVector;\n"
            f"    {match.group(1)}* rotation = outRot != 0 ? outRot : &fallbackRotation;\n"
            f"    {match.group(2)}* vector = outVec != 0 ? outVec : &fallbackVector;" + body)
    # This callback is installed in AttachGetRot, whose host ABI is
    # (self, time, rotation, vector). The matching definition follows the
    # retail EE register convention and puts time last. Reorder only the
    # staged host definition; leave the decomp source and public declaration
    # untouched.
    host_signature = (
        f"void CombineChildPoseRotations(ChildPoseSource* self, f32 t, "
        f"{match.group(1)}* outRot, {match.group(2)}* outVec)\n{{")
    return text[:match.start()] + host_signature + body + text[end - 1:]


def emit_text_layout_setters_for_host(text, label):
    """Keep the old GCC pragma-interface setters linkable on modern hosts."""
    if label != "main/text_panel.cpp":
        return text
    text, count = re.subn(
        r"(?m)^inline (void TextBoxLayout::(?:Pos|Size|Color|SetF14|SetF18)\()",
        r"\1", text)
    if count != 5:
        raise RuntimeError("expected five TextBoxLayout setter definitions")
    return text


def normalize_active_set_solver_abi(text, label):
    """Preserve the legacy host ABI shim only for the older decomp signature."""
    old = (
        "AddFrictionItemsAndSolve(ActiveSetSolver* self, s32 a1, s32 n, "
        "BodyContact** contacts, s32 a4, s32 a5, f32 dt)")
    current = (
        "AddFrictionItemsAndSolve(ActiveSetSolver* self, s32 a1, s32 n, "
        "BodyContact** contacts, s32* sizes, void** lists, f32 dt)")
    new = (
        "AddFrictionItemsAndSolve(f32 dt, ActiveSetSolver* self, s32 a1, "
        "s32 n, BodyContact** contacts, s32 a4, s32 a5)")
    if label == "functions/active_set_solver.h":
        if text.count("void " + current + ";") == 1:
            # The current decomp also corrected the caller to use this
            # declaration, so reordering arguments would now break the ABI.
            return text
        if text.count("void " + old + ";") != 1:
            raise RuntimeError("expected active-set solver host ABI declaration")
        return text.replace("void " + old + ";", "void " + new + ";", 1)
    if label == "main/active_set_solver.cpp":
        if text.count("void " + current + "\n{") == 1:
            return text
        if text.count("void " + old + "\n{") != 1:
            raise RuntimeError("expected active-set solver host ABI definition")
        return text.replace("void " + old + "\n{", "void " + new + "\n{", 1)
    return text


def bridge_run_main_loop_linkage(text, label):
    """Keep the native launcher call stable across RunMainLoop linkage edits."""
    if label != "main/main_loop.cpp":
        return text
    signature = 'extern "C" void RunMainLoop(s32 a, s32 b)'
    bridge = (
        '\nextern "C" void sly1_native_run_main_loop_c(int, int) '
        'asm("func_00185758");\n'
        'void sly1_native_run_main_loop_cpp(int, int) '
        'asm("_Z13func_00185758ii");\n'
        'void sly1_native_run_main_loop_cpp(int a, int b) { '
        'sly1_native_run_main_loop_c(a, b); }\n')
    if signature not in text:
        return text
    if "sly1_native_run_main_loop_c" in text:
        return text
    return text + bridge


def normalize_uncached_ee_cpu_pointers(text, label):
    """Use mapped EE addresses when host CPU code dereferences uncached aliases."""
    replacements = {
        "main/object_properties.cpp": (
            "    t.tags = (DmaTag*)((u32)(i != 0 ? g_sprChainTagsB : g_sprChainTagsA) | 0x20000000);",
            "    t.tags = (DmaTag*)(i != 0 ? g_sprChainTagsB : g_sprChainTagsA);",
            "scratchpad transfer tag alias assignment",
        ),
        "main/group_marker.cpp": (
            "        *(DeferredRequest*)((u32)cur | 0x20000000) = *src;",
            "        *cur = *src;",
            "deferred-request uncached pointer store",
        ),
        "main/movie_player.cpp": (
            "    InitRingBuffer(&self->videoRing, 0x30000, (u8*)((s32)g_movieVideoRingBuffer | 0x20000000));",
            "    InitRingBuffer(&self->videoRing, 0x30000, g_movieVideoRingBuffer);",
            "movie ring-buffer uncached pointer",
        ),
    }
    replacement = replacements.get(label)
    if replacement is None:
        return text
    old, new, description = replacement
    if text.count(new) == 1:
        return text
    if text.count(old) != 1:
        raise RuntimeError("expected " + description)
    return text.replace(old, new, 1)


def trace_null_approach_ramp(text, label):
    """Capture a host-only caller trace for null ramp profiles."""
    if label != "main/warp_menu.cpp":
        return text
    old_signature = (
        "f32 ApproachValueWithRamp(RampProfile* k, f32* out, "
        "f32 x, f32 y, f32 z)\n{")
    current_signature = (
        "f32 ApproachValueWithRamp(f32 x, f32 y, f32 z, "
        "RampProfile* k, f32* out)\n{")
    if text.count(old_signature) == 1:
        signature = old_signature
    elif text.count(current_signature) == 1:
        signature = current_signature
    else:
        raise RuntimeError("expected ApproachValueWithRamp definition")
    replacement = (
        'extern "C" void sly1_native_trace_null_ramp(\n'
        "    void* ramp, void* out, f32 x, f32 y, f32 z, u32 caller);\n"
        "__attribute__((noinline))\n"
        + signature
        + "\n    if (k == 0) {\n"
        "        sly1_native_trace_null_ramp(\n"
        "            k, out, x, y, z, (u32)__builtin_return_address(0));\n"
        "    }")
    return text.replace(signature, replacement, 1)


def forward_mesh_contact_radius(text, label):
    if label != "main/collider_contact.cpp":
        return text
    # Current decomp source carries the shared typed prototype and all call
    # sites use the same radius-second order. The reorder below is only for
    # older sources where the radius was implicit in the collider wrapper.
    if re.search(r'(?m)^#include\s+"functions/collider_contact\.h"', text):
        return text
    old_decl = "s32 CollectMeshHits(void* a0, void* a1, void* a2, void* a3, void* a4, s32 a5, MeshHitEntry* a6);"
    new_decl = "s32 CollectMeshHits(void* a0, void* a1, void* a2, void* a3, void* a4, s32 a5, MeshHitEntry* a6, f32 radius);"
    old_call = "CollectMeshHits(a1, a3, a4, a5, (u8*)a2 + 0x3A4, 0x40, ents)"
    new_call = "CollectMeshHits(a1, a3, a4, a5, (u8*)a2 + 0x3A4, 0x40, ents, radius)"
    latest_decl = (
        "s32 CollectMeshHits(Vec02* center, f32 radius, s32 count, FaceRef* list, "
        "FaceRef* cursor, Mesh03* mesh, s32 cap, MeshHitEntry* out)"
    )
    latest_new_decl = (
        "s32 CollectMeshHits(Vec02* center, s32 count, FaceRef* list, "
        "FaceRef* cursor, Mesh03* mesh, s32 cap, MeshHitEntry* out, f32 radius)"
    )
    latest_call = (
        "CollectMeshHits(a1, radius, a3, a4, a5, "
        "(Mesh03*)((u8*)a2 + 0x3A4), 0x40, ents)"
    )
    latest_new_call = (
        "CollectMeshHits(a1, a3, a4, a5, "
        "(Mesh03*)((u8*)a2 + 0x3A4), 0x40, ents, radius)"
    )
    if text.count(old_decl) == 1 and text.count(old_call) == 1:
        return text.replace(old_decl, new_decl, 1).replace(old_call, new_call, 1)
    if text.count(latest_decl) == 1 and text.count(latest_call) == 1:
        return text.replace(latest_decl, latest_new_decl, 1).replace(
            latest_call, latest_new_call, 1)
    raise RuntimeError("expected implicit mesh-contact radius")


def normalize_camera_mesh_contact_abi(text, label):
    """Put the camera's mesh-hit call into the host definition's argument order."""
    if label != "main/camera.cpp":
        return text
    # Current decomp source uses the shared typed signature and the definition
    # keeps radius second, so leave its call in that order.
    if re.search(r'(?m)^#include\s+"functions/collider_contact\.h"', text):
        return text
    old_decl = (
        "s32 CollectMeshHits(void* a0, f32 r, s32 a1, TreeNode03* a2, "
        "s32 a3, void* a4, s32 a5, MeshHit* a6);"
    )
    new_decl = (
        "s32 CollectMeshHits(void* a0, s32 a1, TreeNode03* a2, s32 a3, "
        "void* a4, s32 a5, MeshHit* a6, f32 r);"
    )
    old_call = (
        "CollectMeshHits(a4, r, c->tree2.count2, c->tree2.nodes, 0, "
        "&c->mesh2Copy, 0x40 - total, &hits[total])"
    )
    new_call = (
        "CollectMeshHits(a4, c->tree2.count2, c->tree2.nodes, 0, "
        "&c->mesh2Copy, 0x40 - total, &hits[total], r)"
    )
    latest_decl = (
        "s32 CollectMeshHits(Vec02* center, f32 radius, s32 count, FaceRef* list, "
        "FaceRef* cursor, Mesh03* mesh, s32 cap, MeshHitEntry* out);"
    )
    latest_new_decl = (
        "s32 CollectMeshHits(Vec02* center, s32 count, FaceRef* list, "
        "FaceRef* cursor, Mesh03* mesh, s32 cap, MeshHitEntry* out, f32 radius);"
    )
    latest_call = (
        "CollectMeshHits((Vec02*)a4, r, c->tree2.count2, "
        "(FaceRef*)c->tree2.nodes, 0, &c->mesh2Copy, 0x40 - total, "
        "(MeshHitEntry*)&hits[total])"
    )
    latest_new_call = (
        "CollectMeshHits((Vec02*)a4, c->tree2.count2, "
        "(FaceRef*)c->tree2.nodes, 0, &c->mesh2Copy, 0x40 - total, "
        "(MeshHitEntry*)&hits[total], r)"
    )
    if text.count(old_decl) == 1 and text.count(old_call) == 1:
        return text.replace(old_decl, new_decl, 1).replace(old_call, new_call, 1)
    if text.count(latest_decl) == 1 and text.count(latest_call) == 1:
        return text.replace(latest_decl, latest_new_decl, 1).replace(
            latest_call, latest_new_call, 1)
    raise RuntimeError("expected camera mesh-hit ABI call")


def trace_oscillator_outputs(text, label, enabled):
    """Trap invalid oscillator vertex counts before they can overwrite the host stack."""
    if label != "main/oscillator.cpp" or not enabled:
        return text
    range_target = "            s32 n = (r->end - r->start) / r->stride;"
    range_replacement = range_target + """
            if (n < 0 || n > 0x100) {
                // Record the count, range record, packed bounds and stride
                // in saved registers without calling out of the render thread.
                const u32 range_limits = ((u32)r->start << 16) | r->end;
                const u32 range_stride = r->stride;
                __asm__ volatile("ud2" : :
                    "a"(n), "b"((u32)r), "c"(range_limits), "d"(range_stride) :
                    "memory");
                __builtin_unreachable();
            }"""
    if text.count(range_target) != 1:
        raise RuntimeError("expected oscillator vertex range count")
    return text.replace(range_target, range_replacement, 1)


def index_host_oscillator_vertex_loops(text, label):
    """Keep VU0-heavy host loops from reusing clobbered index registers."""
    if label != "main/oscillator.cpp":
        return text
    object_pointer = "        PlacedObject* obj = part->obj;"
    volatile_object_pointer = "        PlacedObject* volatile obj = part->obj;"
    mesh_pointer = "        OscMesh* mesh = part->mesh;"
    volatile_mesh_pointer = "        OscMesh* volatile mesh = part->mesh;"
    weight_loop = "            for (v = verts; v < end; v++, w++) {"
    weight_indexed = """            for (volatile s32 sly1_vertex_i = 0;
                 sly1_vertex_i < n; ++sly1_vertex_i) {
                v = sly1_native_verts + sly1_vertex_i;
                w = &weights[sly1_vertex_i];"""
    weight_store = "                *w = weight;"
    # Host VU lowering expands the vector math into scalar operations. Keep a
    # compiler memory barrier before each indexed access so GCC reloads the
    # volatile loop index after that math instead of reusing a scratch register.
    indexed_weight_store = """                __asm__ volatile("" ::: "memory");
                w = &weights[sly1_vertex_i];
                *w = weight;"""
    output_loop = "                for (v = verts; v < end; v++, out++, w++) {"
    output_indexed = """                for (volatile s32 sly1_vertex_i = 0;
                     sly1_vertex_i < n;
                     ++sly1_vertex_i, ++out) {
                    v = sly1_native_verts + sly1_vertex_i;
                    w = &weights[sly1_vertex_i];"""
    output_weight_reload = ("                    u128 qc = Vu0::Copy(q);\n"
                            "                    __asm__ volatile(\"\" ::: \"memory\");\n"
                            "                    w = &weights[sly1_vertex_i];")
    copy_target = re.compile(
        r"(?m)^(            CopyMemory\(verts, data->verts, n \* sizeof\((Vec02|Vec4)\)\);)$"
    )
    copy_matches = list(copy_target.finditer(text))
    if (text.count(object_pointer) != 1 or text.count(mesh_pointer) != 1 or
            text.count(weight_loop) != 1 or
            text.count(output_loop) != 1 or text.count(weight_store) != 1):
        raise RuntimeError("expected oscillator vertex loops")
    if len(copy_matches) != 1 or text.count("                    u128 qc = Vu0::Copy(q);\n") != 1:
        raise RuntimeError("expected oscillator vertex copy")
    copy_line = copy_matches[0].group(1)
    vector_type = copy_matches[0].group(2)
    copy_adapted = (copy_line
                    + f"\n            {vector_type}* volatile sly1_native_verts = verts;")
    adapted = (text.replace(object_pointer, volatile_object_pointer, 1)
               .replace(mesh_pointer, volatile_mesh_pointer, 1)
               .replace(copy_line, copy_adapted, 1)
               .replace(weight_loop, weight_indexed, 1)
               .replace(weight_store, indexed_weight_store, 1)
               .replace(output_loop, output_indexed, 1)
               .replace("                    u128 qc = Vu0::Copy(q);\n",
                        output_weight_reload, 1))
    # At -O2 the translated vector math keeps the loop index in a register
    # that it also uses as a vector scratch value. This is a host-only compiler
    # workaround; the matched source and its behavior remain unchanged.
    adapted = adapted.replace(
        "void RefreshOscillatorFrames(Oscillator* self)\n{\n",
        '__attribute__((optimize("O1")))\nvoid RefreshOscillatorFrames(Oscillator* self)\n{\n',
        1)
    return adapted


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

    # Struct members and function locals must keep their declared layout.
    # In particular, aligning ModelStrip::keys changes the record from its
    # retail 0x50 bytes and makes the host compiler index past the allocation.
    # Only align file-scope arrays (also allowing namespace and extern-C
    # scopes, which contain file-scope declarations).
    result = []
    scopes = []
    for line in text.splitlines(keepends=True):
        file_scope = all(scope in ("namespace", "linkage") for scope in scopes)
        result.append(pattern.sub(replace, line) if file_scope else line)
        scrubbed = re.sub(r'//.*|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'', "", line)
        opens = scrubbed.count("{")
        closes = scrubbed.count("}")
        if opens:
            if re.match(r"\s*namespace(?:\s+\w+)?\s*\{", scrubbed):
                kind = "namespace"
            elif re.match(r'\s*extern\s+"C"\s*\{', line):
                kind = "linkage"
            else:
                kind = "other"
            scopes.extend([kind] * opens)
        for _ in range(min(closes, len(scopes))):
            scopes.pop()
    return "".join(result)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", required=True, type=Path, help="read-only sly1-decomp checkout")
    parser.add_argument("--out", required=True, type=Path, help="staged source and object output directory")
    parser.add_argument("--port", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--cxx", default="c++")
    parser.add_argument("--trace-functions", action="store_true",
                        help="add targeted startup state traces without instrumenting game calls")
    parser.add_argument("--trace-main-loop", action="store_true",
                        help="trace only startup and frame stages in RunMainLoop")
    parser.add_argument("--trace-model-draws", action="store_true",
                        help="trace DrawModelPart inputs in the staged host source")
    parser.add_argument("--native-sly-world-renderer", action="store_true",
                        help="bridge compiled kind-6 Sly model meshes to the native renderer")
    parser.add_argument("--trace-world-draw-lists", action="store_true",
                        help="trace active-world draw-list heads and callback targets")
    parser.add_argument("--trace-deferred-requests", action="store_true",
                        help="trace callback types submitted by the active world")
    parser.add_argument("--trace-oscillator", action="store_true",
                        help="probe RefreshOscillatorFrames without enabling other probes")
    parser.add_argument("--trace-placed-object-load", action="store_true",
                        help="trace serialized rotation and lifecycle transforms for a placed object")
    args = parser.parse_args(argv)

    source = args.source.expanduser().resolve()
    out = args.out.expanduser().resolve()
    port = args.port.resolve()
    include = source / "include"
    names_header = include / "names.h"
    ramp_header = include / "functions" / "warp_menu.h"
    ramp_header_text = ramp_header.read_text(encoding="utf-8") if ramp_header.is_file() else ""
    legacy_ramp_abi = (
        "ApproachValueWithRamp(RampProfile* k, f32* out, f32 x, f32 y, f32 z)"
        in ramp_header_text)
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
        if header.suffix in {".h", ".hpp"}:
            header_text = header.read_text(encoding="utf-8")
            staged_header.write_text(normalize_gif_register_writer_abi(header_text),
                                     encoding="utf-8")
        else:
            shutil.copy2(header, staged_header)
    solver_header = staged_include / "functions/active_set_solver.h"
    solver_header.write_text(
        normalize_active_set_solver_abi(
            solver_header.read_text(encoding="utf-8"),
            "functions/active_set_solver.h"),
        encoding="utf-8")
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
        # Keep line tables in every staged unit so host crashes (e.g. the
        # intermittent mesh-submit double-free) can be resolved to a source
        # line with addr2line. -g does not change generated code or ABI.
        "-g", "-O2",
        "-fno-pie", "-fno-pic", "-ffunction-sections", "-fdata-sections",
        "-fno-asynchronous-unwind-tables", "-fno-unwind-tables", "-fpermissive",
        "-flax-vector-conversions",
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
        adapted = return_appended_call_record(adapted, rel.as_posix())
        if rel.as_posix() == "main/text_panel.cpp":
            # These generated Layout methods are inline in the matching C++
            # but their retail entry points are called from other files.
            # Emit host symbols for the address table without changing the
            # decompilation source.
            adapted = re.sub(
                r"(?m)^inline (void Layout_137228::(?:Pos|Size|Color|SetF14|SetF18)\()",
                r"\1", adapted)
        adapted = forward_load_collider_stream(adapted, rel.as_posix())
        adapted = bridge_run_main_loop_linkage(adapted, rel.as_posix())
        adapted = forward_placed_object_load_stream(adapted, rel.as_posix())
        adapted = forward_animation_marker_arguments(adapted, rel.as_posix())
        adapted = forward_blend_entry_flag_arguments(adapted, rel.as_posix())
        adapted = canonicalize_hermite_vector_calls(adapted, rel.as_posix())
        adapted = forward_keyed_rotation_get_arguments(adapted, rel.as_posix())
        adapted = refresh_attachment_target_matrix_context(adapted, rel.as_posix())
        adapted = preserve_postload_attachment_context(adapted, rel.as_posix())
        adapted = supply_player_collider_position(adapted, rel.as_posix())
        adapted = normalize_turning_collider_matrix_abi(adapted, rel.as_posix())
        adapted = normalize_agent_axis_angle_matrix_abi(adapted, rel.as_posix())
        adapted = normalize_link_constraint_axis_angle_matrix_abi(
            adapted, rel.as_posix())
        adapted = make_axis_curve_outputs_unaligned_safe(adapted, rel.as_posix())
        adapted = normalize_attachment_matrix_value_abi(adapted, rel.as_posix())
        adapted = normalize_colored_sprite_packet_abi(adapted, rel.as_posix())
        adapted = normalize_placed_object_sound_abi(adapted, rel.as_posix())
        adapted = normalize_script_host_call_abi(adapted, rel.as_posix())
        adapted = make_point_frame_transform_unaligned_safe(adapted, rel.as_posix())
        adapted = align_collider_position_argument(adapted, rel.as_posix())
        adapted = make_null_memory_copy_deterministic(adapted, rel.as_posix())
        adapted = map_null_turning_collider_face_reads(adapted, rel.as_posix())
        adapted = rebuild_collider_mesh_motion_hook(adapted, rel.as_posix())
        adapted = trace_collider_mesh_motion_allocations(adapted, rel.as_posix())
        adapted = trace_turning_mesh_hook_use(adapted, rel.as_posix())
        adapted = trace_turning_mesh_copy_state(adapted, rel.as_posix())
        adapted = trace_arrival_lookup(adapted, rel.as_posix())
        adapted = return_from_world_transform_wrappers(adapted, rel.as_posix())
        adapted = normalize_player_probe_call_abi(adapted, rel.as_posix())
        adapted = forward_ground_contact_arguments(adapted, rel.as_posix())
        adapted = forward_player_slide_contact_index(adapted, rel.as_posix())
        adapted = trace_contact_notification_callback(adapted, rel.as_posix())
        adapted = trace_camera_target_callback(adapted, rel.as_posix())
        adapted = forward_switched_pose_arguments(
            adapted, rel.as_posix(), legacy_ramp_abi)
        adapted = normalize_update_camera_target_pose_abi(adapted, rel.as_posix())
        adapted = normalize_on_off_camera_ramp_abi(
            adapted, rel.as_posix(), legacy_ramp_abi)
        adapted = normalize_turning_collider_ramp_abi(
            adapted, rel.as_posix(), legacy_ramp_abi)
        adapted = normalize_pose_source_ramp_abi(
            adapted, rel.as_posix(), legacy_ramp_abi)
        adapted = normalize_optional_owner_position_outputs(adapted, rel.as_posix())
        adapted = normalize_optional_child_rotation_outputs(adapted, rel.as_posix())
        adapted = forward_mesh_contact_radius(adapted, rel.as_posix())
        adapted = normalize_camera_mesh_contact_abi(adapted, rel.as_posix())
        adapted = make_rotation_translation_matrix_unaligned_safe(
            adapted, rel.as_posix())
        adapted = index_host_oscillator_vertex_loops(adapted, rel.as_posix())
        adapted = trace_null_approach_ramp(adapted, rel.as_posix())
        adapted = trace_oscillator_outputs(
            adapted, rel.as_posix(), args.trace_functions or args.trace_oscillator)
        if rel.as_posix() == "lib/system_calls.cpp":
            # The US kernel veneers execute EE `syscall` instructions, which
            # cannot be used by the direct native build. Route only the
            # translated thread/semaphore veneers to separate port services;
            # the matching source in sly1-decomp remains unchanged.
            services = {
                "func_001F6A10": ("sly1_native_make_thread", "const void* parameters", "parameters"),
                "func_001F6A20": ("sly1_native_start_thread", "int id, void* argument", "id, argument"),
                "func_001F6A50": ("sly1_native_set_thread_priority", "int id, int priority", "id, priority"),
                "func_001F6A90": ("sly1_native_get_thread_id", "void", ""),
                "func_001F6B20": ("sly1_native_create_semaphore", "const void* parameters", "parameters"),
                "func_001F6B30": ("sly1_native_delete_semaphore", "int id", "id"),
                "func_001F6B40": ("sly1_native_signal_semaphore", "int id", "id"),
                "func_001F6B50": ("sly1_native_signal_semaphore", "int id", "id"),
                "func_001F6B60": ("sly1_native_wait_semaphore", "int id", "id"),
                "func_001F6B70": ("sly1_native_poll_semaphore", "int id", "id"),
                "func_001F6B80": ("sly1_native_poll_semaphore", "int id", "id"),
            }
            declarations = []
            for name, (service, parameters, arguments) in services.items():
                declaration = f"ASM_FUNC({name});"
                if adapted.count(declaration) != 1:
                    raise RuntimeError("expected one retail kernel veneer " + name)
                declarations.append(
                    f'extern "C" int {service}({parameters});\n'
                    f'extern "C" int {name}({parameters}) {{ return {service}('
                    f'{arguments}); }}'
                )
                adapted = adapted.replace(declaration, declarations[-1], 1)
        if rel.as_posix() == "main/warp_menu.cpp":
            # The decomp now describes ElementStack as a 0x34-byte structure
            # in screen_element.h, but its global backing array still has a
            # four-byte placeholder. Runtime initialization and frame updates
            # use the complete structure, so the placeholder overlaps adjacent
            # host BSS objects. Size only the staged host definition from the
            # recovered layout; keep matching source and score untouched.
            declaration = "u8 g_elementStack[4] = {0};"
            if adapted.count(declaration) != 1:
                raise RuntimeError("expected one ElementStack backing array in " +
                                   rel.as_posix())
            adapted = adapted.replace(
                declaration,
                "u8 g_elementStack[sizeof(ElementStack)] = {0};")
        adapted = forward_copy_callback_source(adapted, rel.as_posix())
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
            # Opt-in: trace PollPadInput calls and the buttons it decodes, to
            # check whether the game's pad state machine reaches the poll and
            # whether injected input lands in PadRecord.buttons.
            adapted = ('extern "C" void sly1_native_trace_poll_pad(u32,u32,u32);\n') + adapted
            poll_needle = "        self->flags = cur & ~old;"
            if adapted.count(poll_needle) == 1:
                adapted = adapted.replace(poll_needle,
                    "        sly1_native_trace_poll_pad((u32)self, self->buttons, res);\n" + poll_needle, 1)
        elif rel.as_posix() == "main/curve_segment.cpp":
            # FindEarliestTimeWithinCurveLimits walks the PS2 EE's 8-byte vararg
            # slots by hand (__builtin_next_arg - 56, 8-byte steps). On native
            # i386 those slots are garbage, so the forced level load faults here
            # (c pointer is invalid). Rebuild the vararg walk with host <cstdarg>:
            # each pair is a curve pointer and a double time limit, then the
            # result pointer. The matched decompilation source is untouched.
            cursor_line = "    char* ap = (char*)__builtin_next_arg(n) - 56;"
            if adapted.count(cursor_line) == 1:
                adapted = adapted.replace(
                    cursor_line,
                    "    __builtin_va_list ap;\n    __builtin_va_start(ap, n);")
            adapted = adapted.replace(
                "        f32* c = *(f32**)((ap += 8) - 8);\n"
                "        f32 t = ConvertDoubleToFloat(*(u64*)((ap += 8) - 8));",
                "        f32* c = __builtin_va_arg(ap, f32*);\n"
                "        double t64 = __builtin_va_arg(ap, double);\n"
                "        f32 t = (f32)t64;")
            adapted = adapted.replace(
                "    **(f32**)((ap += 8) - 8) = best;\n}",
                "    *__builtin_va_arg(ap, f32*) = best;\n    __builtin_va_end(ap);\n}")
        if rel.as_posix() == "main/player.cpp":
            # Port-only entry shim for the forced-level pad fault. On that path
            # the retail ASM caller reaches ControlPlayer with arg2/arg3 swapped:
            # pad holds a float (~0.033, a dt) and dt holds the PadRecord pointer.
            # Detect that (pad is not a mapped pointer, dt bits are) and swap the
            # two via local aliases so the decomp body sees the intended values.
            # The normal path passes valid args and is untouched.
            needle = "void ControlPlayer(Player* self, PadRecord* pad, f32 dt)\n{"
            shim = (
                "void ControlPlayer(Player* self, PadRecord* pad_in, f32 dt_in)\n{\n"
                "    u32 pad_bits = (u32)pad_in;\n"
                "    u32 dt_bits = *(u32*)&dt_in;\n"
                "    PadRecord* pad = pad_in;\n"
                "    f32 dt = dt_in;\n"
                "    if ((pad_bits < 0x00010000u || pad_bits >= 0x02000000u) &&\n"
                "        ((dt_bits >= 0x00010000u && dt_bits < 0x02000000u) ||\n"
                "         (dt_bits >= 0x08000000u && dt_bits < 0x10000000u))) {\n"
                "        dt = *(f32*)&pad_bits;\n"
                "        pad = (PadRecord*)dt_bits;\n"
                "    }")
            if adapted.count(needle) == 1:
                adapted = adapted.replace(needle, shim, 1)
            # Opt-in entry trace: is ControlPlayer reached in the gameplay state?
            adapted = ('extern "C" void sly1_native_trace_control_player(u32,u32,u32);\n') + adapted
            entry = "        pad = (PadRecord*)dt_bits;\n    }"
            if adapted.count(entry) == 1:
                adapted = adapted.replace(entry,
                    entry + "\n    sly1_native_trace_control_player((u32)self, (u32)pad, *(u32*)&dt);", 1)
            # Dump the pad's button word (bytes 2/3) seen by ControlPlayer.
            adapted = ('extern "C" void sly1_native_trace_player_pad_buttons(u32,u32,u32);\n') + adapted
            if adapted.count(entry) == 1:
                adapted = adapted.replace(entry,
                    entry + "\n    if (pad != 0) sly1_native_trace_player_pad_buttons(*(u16*)((u8*)pad + 0xA8u), *(u32*)((u8*)pad + 0x0Cu), pad->buffer != 0 ? *(u32*)pad->buffer : 0u);", 1)
        elif rel.as_posix() == "main/turning_collider.cpp":
            # Opt-in: log the two pointers at ReadTurningColliderStickHeading
            # entry to diagnose the forced-level post-load pad fault.
            adapted = ('extern "C" void sly1_native_trace_turn_heading(u32, u32);\n') + adapted
            needle = ("void ReadTurningColliderStickHeading(TurningCollider* self, PadRecord* pad)\n{")
            repl = (needle + "\n    sly1_native_trace_turn_heading((u32)self, (u32)pad);")
            if adapted.count(needle) == 1:
                adapted = adapted.replace(needle, repl, 1)
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
            if rel.as_posix() == "main/object_properties.cpp":
                # StartFromSprChain also walks the EE's variadic home area by
                # hand. On i386, start at the first host vararg and step in
                # 4-byte slots, or it reads beyond the zero terminator and
                # writes a DMA tag just before scratchpad.
                old_cursor = "void* ap = (char*)__builtin_next_arg(first) - 56;"
                new_cursor = "void* ap = (char*)__builtin_next_arg(first);"
                if old_cursor in adapted:
                    adapted = adapted.replace(old_cursor, new_cursor, 1)
                elif new_cursor not in adapted:
                    raise RuntimeError("expected StartFromSprChain host vararg cursor")
                old_step = "        ap = (char*)ap + 8;\n        x = *(s32*)((char*)ap - 8);"
                new_step = "        ap = (char*)ap + 4;\n        x = *(s32*)((char*)ap - 4);"
                if old_step in adapted:
                    adapted = adapted.replace(old_step, new_step, 1)
                elif new_step not in adapted:
                    raise RuntimeError("expected StartFromSprChain host vararg step")
                # StoreScriptArgumentsVa uses the EE's 8-byte vararg slots.
                # Use the host i386 varargs ABI in this staging copy; otherwise
                # it reads unrelated stack words as output destinations.
                old = '''ScriptValue* StoreScriptArgumentsVa(ScriptValue* ret, s32 count, ScriptValue* args, const void* kinds, ...)
{
    void* arr[32];
    void* ap = (char*)__builtin_next_arg(kinds) - 32;
    if (count > 0) {
        void** d = arr;
        s32 i = count;
        do {
            ap = (char*)ap + 8;
            *d = *(void**)((char*)ap - 8);
            d++;
            i--;
        } while (i != 0);
    }
    StoreScriptArguments(ret, count, args, (const u32*)kinds, arr);
    return ret;
}'''
                new = '''ScriptValue* StoreScriptArgumentsVa(ScriptValue* ret, s32 count, ScriptValue* args, const void* kinds, ...)
{
    void* arr[32];
    __builtin_va_list ap;
    __builtin_va_start(ap, kinds);
    for (s32 i = 0; i < count; ++i) arr[i] = __builtin_va_arg(ap, void*);
    __builtin_va_end(ap);
    StoreScriptArguments(ret, count, args, (const u32*)kinds, arr);
    return ret;
}'''
                if adapted.count(old) != 1:
                    raise RuntimeError("expected EE StoreScriptArgumentsVa body")
                adapted = adapted.replace(old, new, 1)
        elif rel.as_posix() in ("main/file_1E88D0.cpp", "main/warp_menu.cpp"):
            # AreFloatsNearlyEqual has a separate native replacement; retain
            # its public address prototype while renaming the upstream body.
            adapted = adapted.replace(
                '#include "common.h"',
                '#include "common.h"\n'
                'extern "C" s32 func_001EAC68(f32, f32, f32);', 1)
            if (rel.as_posix() == "main/warp_menu.cpp" and
                    "void ComputeSinCos(f32* sinOut, f32* cosOut, f32 angle)\n{" in adapted):
                # This is the actual C ABI of the matched helper. Some callers
                # preserve the EE register order (angle, outputs); rewrite
                # those host-only call sites to the function's pointer-first
                # signature instead of changing the function body ABI.
                retail_call = (
                    "((void (*)(f32, f32*, f32*))ComputeSinCos)"
                    "(WrapAngle(angle), &loc.x, &loc.y);")
                host_call = "ComputeSinCos(&loc.x, &loc.y, WrapAngle(angle));"
                if retail_call in adapted:
                    adapted = adapted.replace(retail_call, host_call, 1)
                else:
                    adapted = adapted.replace(
                        "void ComputeSinCos(f32 angle, f32* sinOut, f32* cosOut);",
                        "void ComputeSinCos(f32* sinOut, f32* cosOut, f32 angle);")
                    adapted, reordered_sincos = re.subn(
                        r"ComputeSinCos\(([^,]+),\s*(&[^,]+),\s*(&[^)]+)\);",
                        r"ComputeSinCos(\2, \3, \1);", adapted)
                    if reordered_sincos != 5:
                        raise RuntimeError("expected five angle-first ComputeSinCos calls")
            else:
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
            if replacements == 0 and adapted.count("size = WriteVifUnpackHeader(s, a, size, d);") == 1:
                pass
            elif replacements != 1:
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
            old_call = "ConvertHsvToRgb(&tmp);"
            new_call = "ConvertHsvToRgb(&tmp, (Quad_001B4580*)dst);"
            replacements = adapted.count(old_call)
            if replacements == 1:
                adapted = adapted.replace(old_call, new_call, 1)
            elif replacements != 0 or adapted.count(new_call) != 1:
                raise RuntimeError("expected one HSV output register call in " +
                                   rel.as_posix())
        elif rel.as_posix() == "main/world_listed_object.cpp":
            # A port-only particle-filter backfill replaces the matching
            # function body in this staged unit. Keep the public declaration
            # available to its later call site after renaming that body.
            declaration = (
                "extern f32 g_gameTime;\n"
                "s32 func_0013A498(const f32*, u128, Vec02*, Vec02*, Vec02*, "
                "s32, s32, void*, void*);\n")
            anchor = "extern f32 g_gameTime;\n"
            if backfill_root.joinpath("func_0013A498.cpp").is_file():
                if adapted.count(anchor) != 1:
                    raise RuntimeError("expected particle filter host declaration point")
                adapted = adapted.replace(anchor, declaration, 1)
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
            # Port-only trace of object creation during resource loading. Log
            # every CreateTypedRecord class index so the loader's handling of
            # the Splash notifier (class index 0x57) can be diagnosed. The host
            # function gates on an env var and is a no-op otherwise.
            adapted = ('extern "C" void sly1_native_trace_create_typed_record('
                       'u32, u32, u32, u32);\n') + adapted
            create_needle = (
                "BaseObject* CreateTypedRecord(s32 index, void* a1, BaseObject* a2, s32 a3, s32 a4)\n{\n"
                "    s32 off = index << 2;")
            create_replacement = (
                "BaseObject* CreateTypedRecord(s32 index, void* a1, BaseObject* a2, s32 a3, s32 a4)\n{\n"
                "    s32 off = index << 2;")
            if adapted.count(create_needle) == 1:
                adapted = adapted.replace(create_needle, create_replacement, 1)
            # Log the created object once it has a descriptor and id, so the
            # notifier's lifetime can be followed (class index 0x57).
            create_end = "    r->desc->m00(r);\n    return r;\n}"
            create_end_new = ("    r->desc->m00(r);\n"
                              "    sly1_native_trace_create_typed_record((u32)index, (u32)r, (u32)a1, (u32)a4);\n"
                              "    return r;\n}")
            if adapted.count(create_end) == 1:
                adapted = adapted.replace(create_end, create_end_new, 1)
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
            # GCC 2.95 walks backward from the separate `ret` local through
            # the 32-element args array. Comparing those unrelated pointers
            # is undefined in modern C++, and host GCC removes the stop test,
            # walking off the stack. Keep the retail order with defined array
            # indexing in the staging copy.
            old_cleanup = """        e = rp;
        while (sc.buf != e) {
            e--;
            DestroyScriptValue(e, 0);
        }"""
            new_cleanup = """        for (s32 cleanup = 32; cleanup != 0; --cleanup) {
            DestroyScriptValue(&sc.buf[cleanup - 1], 0);
        }"""
            if adapted.count(old_cleanup) != 1:
                raise RuntimeError("expected property-value cleanup loop")
            adapted = adapted.replace(old_cleanup, new_cleanup, 1)
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
            current = ("void EvaluateAxisPositionCurve(AxisPositionCurve* self, "
                       "PlacedObject* other, s32 c, f32* e, f32* f, f32 p12, f32 p13)\n{\n"
                       "    EvaluateAxisCurves(self->axis, other, c, &self->base, e, f, p12, p13);")
            if adapted.count(current) == 1:
                adapted = adapted.replace(current, new, 1)
                adapted = adapted.replace(
                    "((VectorCurveToldFn)other->desc02()->m28)(other, e, f);",
                    "((VectorCurveToldFn)other->desc02()->m28)(other, "
                    "(s32)outValue, (s32)outSlope);", 1)
            elif adapted.count(old) == 1:
                adapted = adapted.replace(old, new, 1)
                adapted = adapted.replace(
                    "((VectorCurveToldFn)other->desc02()->m28)(other, e, f);",
                    "((VectorCurveToldFn)other->desc02()->m28)(other, "
                    "(s32)outValue, (s32)outSlope);", 1)
            else:
                raise RuntimeError("expected one host axis-position callback")
            old = ("void EvaluateVectorKeyPositionCurve(VectorKeyPositionCurve* self, "
                   "PlacedObject* other, s32 a2, s32 a3, s32 a4, f32 p12, f32 p13)\n{\n"
                   "    EvaluateVectorKeyCurve(self->count, (s32)self->keys, a2, a3, a4, p12, p13);")
            new = ("void EvaluateVectorKeyPositionCurve(VectorKeyPositionCurve* self, "
                   "PlacedObject* other, f32 key, f32 scale, s32 mode, "
                   "void* outValue, void* outSlope)\n{\n"
                   "    EvaluateVectorKeyCurve(self->count, (s32)self->keys, mode, "
                   "(s32)outValue, (s32)outSlope, key, scale);")
            current = ("void EvaluateVectorKeyPositionCurve(VectorKeyPositionCurve* self, "
                       "PlacedObject* other, s32 a2, f32* a3, f32* a4, f32 p12, f32 p13)\n{\n"
                       "    EvaluateVectorKeyCurve(self->count, self->keys, a2, a3, a4, p12, p13);")
            if adapted.count(current) == 1:
                adapted = adapted.replace(current, new, 1)
                adapted = adapted.replace(
                    "((VectorCurveToldFn)other->desc02()->m28)(other, a3, a4);",
                    "((VectorCurveToldFn)other->desc02()->m28)(other, "
                    "(s32)outValue, (s32)outSlope);", 1)
            elif adapted.count(old) == 1:
                adapted = adapted.replace(old, new, 1)
                adapted = adapted.replace(
                    "((VectorCurveToldFn)other->desc02()->m28)(other, a3, a4);",
                    "((VectorCurveToldFn)other->desc02()->m28)(other, "
                    "(s32)outValue, (s32)outSlope);", 1)
            else:
                raise RuntimeError("expected one host keyed-position callback")
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
            current = ("void EvaluateAxisRotationCurve(AxisRotationCurve* self, "
                       "PlacedObject* other, s32 c, void* p3, void* p4, "
                       "f32 key, f32 scale)\n{\n"
                       "    AngleSample tmp;\n"
                       "    EvaluateAxisCurves(self->axis, other, c, &self->base, "
                       "p3 ? (f32*)&tmp : 0, p4 ? (f32*)tmp.b : 0, key, scale);")
            if adapted.count(current) == 1:
                pass
            elif adapted.count(old) == 1:
                adapted = adapted.replace(old, new, 1)
            else:
                raise RuntimeError("expected one host rotation-curve callback")
            # Recent decomp sources corrected this helper's declared C ABI to
            # scalar inputs followed by output pointers. Older snapshots used
            # an output-first declaration and need their retail-register calls
            # canonicalized in the staged copy.
            modern_hermite_abi = (
                '#include "functions/curve_segment.h"' in adapted or
                "void EvaluateHermiteSegment(f32 dur, f32 t, f32 e, f32 a, "
                "f32 b, f32 c, f32 dd, f32* p0, f32* p1, f32* p2);" in adapted)
            if modern_hermite_abi:
                second = len(re.findall(
                    r"EvaluateHermiteSegment\(left, dt, 1\.0f, "
                    r"owner->linkRecord->angle, owner->linkRecord->angleSlope, "
                    r"value, slope, &owner->linkRecord->angle, "
                    r"&owner->linkRecord->angleSlope, 0\);",
                    adapted))
                if second != 1:
                    raise RuntimeError("expected one modern Hermite call in " +
                                       rel.as_posix())
            else:
                adapted = adapted.replace(
                    "void EvaluateHermiteSegment(f32 delta, f32 diff, f32 scale, "
                    "f32 p4, f32 p14, f32 p1C, f32 p24, f32* outF, f32* outI, "
                    "s32 zero);",
                    "void EvaluateHermiteSegment(f32* outF, f32* outI, f32* out2, "
                    "f32 delta, f32 diff, f32 scale, f32 p4, f32 p14, f32 p1C, "
                    "f32 p24);")
                adapted = adapted.replace(
                    "void EvaluateHermiteSegment(f32 dur, f32 t, f32 e, f32 a, "
                    "f32 b, f32 c, f32 dd, f32* p0, f32* p1, f32* p2);",
                    "void EvaluateHermiteSegment(f32* p0, f32* p1, f32* p2, "
                    "f32 dur, f32 t, f32 e, f32 a, f32 b, f32 c, f32 dd);")
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
                    r"owner->linkRecord->angle, owner->linkRecord->angleSlope, "
                    r"value, slope, &owner->linkRecord->angle, "
                    r"&owner->linkRecord->angleSlope, 0\);",
                    "EvaluateHermiteSegment(&owner->linkRecord->angle, "
                    "&owner->linkRecord->angleSlope, 0, left, dt, 1.0f, "
                    "owner->linkRecord->angle, owner->linkRecord->angleSlope, "
                    "value, slope);",
                    adapted)
                if replacements != 1 or second != 1:
                    raise RuntimeError("expected two legacy Hermite calls in " +
                                       rel.as_posix())
        elif rel.as_posix() == "main/pooled_object.cpp":
            # Opt-in trace of QueuePlacedObjectDraw: log the object, its draw
            # range and part count to see why so few objects reach DrawModelPart.
            adapted = ('extern "C" void sly1_native_trace_queue_draw(u32, u32, u32, u32);\n') + adapted
            qneedle = ("void QueuePlacedObjectDraw(TextureRenderObject* self, s32 unused, void* a2)\n{\n"
                       "    (void)unused;\n"
                       "    if (self->drawRange != 0) {")
            qrepl = ("void QueuePlacedObjectDraw(TextureRenderObject* self, s32 unused, void* a2)\n{\n"
                     "    (void)unused;\n"
                     "    sly1_native_trace_queue_draw((u32)self, (u32)self->drawRange, "
                     "(u32)self->drawPartCount, (u32)g_currentWorld);\n"
                     "    if (self->drawRange != 0) {")
            if adapted.count(qneedle) == 1:
                adapted = adapted.replace(qneedle, qrepl, 1)
            adapted = adapted.replace(
                "void* GetListLinks(void* a, void* b);",
                "ListLink* GetListLinks(ObjectList* a, void* b);")
            adapted = adapted.replace(
                "u32 GetListLinks(ObjectList* node, u32 value);\n", "")
            # InitStepSequence calls the PS2 method InitPlacedObject with an
            # implicit this pointer in a0. The host C ABI needs that pointer
            # as an explicit argument, or the base initializer reads stale
            # stack data as its object.
            old = "void InitStepSequence(StepSequence* state)\n{\n\tInitPlacedObject();"
            new = ("void InitStepSequence(StepSequence* state)\n{\n"
                   "\tInitPlacedObject((void*)state);")
            if "void InitStepSequence(StepSequence* state)\n{\n\tInitPlacedObject(state);" in adapted:
                old = ""
            if not old:
                pass
            elif adapted.count(old) == 1:
                adapted = adapted.replace(old, new, 1)
            else:
                raise RuntimeError("expected InitStepSequence base-init call")
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
                old_count = adapted.count(old)
                new_count = adapted.count(new)
                if old_count == 1:
                    adapted = adapted.replace(old, new, 1)
                elif old_count == 0 and new_count == 1:
                    continue
                else:
                    raise RuntimeError("expected one keyed-entry ABI site in " +
                                       rel.as_posix() + ": " + old.splitlines()[0])
        elif rel.as_posix() == "main/base_object.cpp":
            # Opt-in trace of script events dispatched to the Splash state graph
            # (class 0x88, id 0x25F), to see which events fire and whether the
            # graph has a script scope / handler for them.
            adapted = ('extern "C" void sly1_native_trace_graph_event('
                       'u32, u32, u32);\n') + adapted
            handler_needle = "void RunScriptEventHandler(BaseObject* self, s32 key, R11B008Node* list)\n{"
            handler_trace = handler_needle + """
    if (self != 0 && self->desc != 0 &&
        *(s32*)((u8*)self->desc + 4) == 0x88 && self->id == 0x25F)
        sly1_native_trace_graph_event((u32)key,
            *(u32*)((u8*)self + 0x2Cu), (u32)list);
"""
            if adapted.count(handler_needle) == 1:
                adapted = adapted.replace(handler_needle, handler_trace, 1)
            # The reworked matching source now owns both helpers. Its second
            # overload calls the first with output pointers last, while the
            # implementation at 0x00183F40 takes them first. Preserve the
            # intended function and make that argument order explicit for the
            # host compiler; the former adapter targeted a retired backfill.
            current_line_fit = (
                "void FitLineThroughPoints(f32 a, f32 b, f32 c, f32 d, "
                "f32* out0, f32* out1)")
            if current_line_fit not in adapted:
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
            latest_declaration = "void* TakeCallRecord(World* self);"
            latest_call = re.compile(
                r"\w+\*\s+\w+\s*=\s*\(\w+\*\)TakeCallRecord\(world\);"
            )
            if declaration_count == 0 and call_count == 0 and \
                    adapted.count(latest_declaration) == 1 and \
                    len(latest_call.findall(adapted)) == 1:
                pass
            elif declaration_count != 1 or call_count != 1:
                raise RuntimeError("expected callback pool world carryover in " +
                                   rel.as_posix())
            if args.trace_functions:
                # The Splash graph emits script events through this shared
                # path. Record whether event 0x11/0xE exists, queues, and
                # actually runs for graph 0x25F. This observes the retail
                # script path without changing its decisions.
                trace_decl = 'extern "C" void sly1_port_trace_value(u32, u32);\n'
                adapted = trace_decl + adapted
                trigger_pattern = (
                    r"(void TriggerScriptEvent\(BaseObject\* self, s32 idx, "
                    r"s32 flag, (?:s32\*|void\*) out\)\n\{)"
                )
                trigger_trace = """
    const s32 trace_graph = self != 0 && self->desc != 0 &&
        *(s32*)((u8*)self->desc + 4) == 0x88 && self->id == 0x25F;
    if (trace_graph) {
        sly1_port_trace_value(0x183a0001u, (u32)idx);
        sly1_port_trace_value(0x183a0002u, (u32)flag);
        sly1_port_trace_value(0x183a0003u, (u32)self->f2C);
        sly1_port_trace_value(0x183a0004u, (u32)out);
    }
"""
                adapted, trigger_count = re.subn(
                    trigger_pattern, lambda match: match.group(1) + trigger_trace,
                    adapted)
                if trigger_count != 1:
                    raise RuntimeError("expected TriggerScriptEvent definition")
                lookup_miss = """    if (self->f2C == 0 || LookupScriptVariable(self->f2C, idx, 1, &r) == 0) {
        DestroyScriptValue(&r, 2);"""
                lookup_miss_trace = """    if (self->f2C == 0 || LookupScriptVariable(self->f2C, idx, 1, &r) == 0) {
        if (trace_graph) sly1_port_trace_value(0x183a0005u, (u32)idx);
        DestroyScriptValue(&r, 2);"""
                latest_lookup_miss = """    if (self->scriptScope == 0 || LookupScriptVariable((ScriptScope*)self->scriptScope, idx, 1, &r) == 0) {
        if (trace_graph) sly1_port_trace_value(0x183a0005u, (u32)idx);
        DestroyScriptValue(&r, 2);"""
                if adapted.count(lookup_miss) == 1:
                    adapted = adapted.replace(lookup_miss, lookup_miss_trace, 1)
                elif adapted.count(latest_lookup_miss.replace(
                        "        if (trace_graph) sly1_port_trace_value(0x183a0005u, (u32)idx);\n", "")) == 1:
                    adapted = adapted.replace(
                        latest_lookup_miss.replace(
                            "        if (trace_graph) sly1_port_trace_value(0x183a0005u, (u32)idx);\n", ""),
                        latest_lookup_miss, 1)
                else:
                    raise RuntimeError("expected graph script lookup miss path")
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
                latest_eval_body = "    EvalScriptBody(&q3, (R11B008Node*)rec->body, slot);"
                selected_eval_body = (eval_body if adapted.count(eval_body) == 1
                                      else latest_eval_body)
                eval_trace = """    if (trace_graph) sly1_port_trace_value(0x183b0003u, (u32)key);
""" + selected_eval_body
                if adapted.count(selected_eval_body) != 1:
                    raise RuntimeError("expected graph script body evaluation")
                adapted = adapted.replace(selected_eval_body, eval_trace, 1)
        elif rel.as_posix() == "main/state_notifier.cpp":
            # Trace whether the Splash countdown notifier's per-frame update
            # runs. When primed (state 2, timerEnabled, timeLeft 0) it should
            # transition to state 6 / event 0x263. Env-gated host function.
            adapted = ('extern "C" void sly1_native_trace_notifier_timer('
                       'u32, u32, u32, u32);\n') + adapted
            timer_needle = (
                "void CountDownNotifierTimer(CountdownNotifier* self, f32 dt)\n{\n"
                "    UpdateStateNotifier(self, dt);")
            timer_repl = (
                "void CountDownNotifierTimer(CountdownNotifier* self, f32 dt)\n{\n"
                "    sly1_native_trace_notifier_timer((u32)self, (u32)self->state, "
                "(u32)self->timerEnabled, (u32)self->timeLeft);\n"
                "    UpdateStateNotifier(self, dt);")
            if adapted.count(timer_needle) == 1:
                adapted = adapted.replace(timer_needle, timer_repl, 1)
        if rel.as_posix() == "main/state_notifier.cpp" and args.trace_functions:
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
        elif rel.as_posix() == "main/oscillator.cpp" and args.trace_functions:
            # This source now owns the subgroup routines previously traced in
            # file_1F2758.cpp. Keep the probes out of matching source.
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
        sly1_port_trace_value(0x1f437802u, found != 0 ? (u32)found->selected : 0);
        sly1_port_trace_value(0x1f437803u, found != 0 ? (u32)found->state : 0);
        sly1_port_trace_value(0x1f437804u, found != 0 ? *(u32*)&found->fadeTime : 0);
        sly1_port_trace_value(0x1f437805u, found != 0 ? (u32)found->count : 0);
    }
    return found;""")
            adapted = adapted[:find_start] + find_body + adapted[find_end:]

            finish_signature = "void StartObjectSubGroupFinish(ObjectSubGroup* self)\n{"
            finish_trace = finish_signature + """
    static u32 trace_count;
    if (trace_count++ < 24) {
        sly1_port_trace_value(0x1f480801u, (u32)self);
        sly1_port_trace_value(0x1f480802u, self != 0 ? (u32)self->selected : 0);
        sly1_port_trace_value(0x1f480803u, self != 0 ? (u32)self->state : 0);
        sly1_port_trace_value(0x1f480804u, self != 0 ? *(u32*)&self->fadeTime : 0);
        sly1_port_trace_value(0x1f480805u, self != 0 ? (u32)self->count : 0);
        sly1_port_trace_value(0x1f480806u, self != 0 ? (u32)self->f2D8 : 0);
    }
"""
            if adapted.count(finish_signature) != 1:
                raise RuntimeError("expected subgroup finish implementation")
            adapted = adapted.replace(finish_signature, finish_trace, 1)

            update_signature = "void UpdateObjectSubGroup(ObjectSubGroup* self, f32 dt)\n{"
            update_trace = update_signature + """
    static u32 trace_count;
    if (self != 0 && self->state == 5 && trace_count++ % 60 == 0) {
        sly1_port_trace_value(0x1f4c0101u, (u32)self);
        sly1_port_trace_value(0x1f4c0102u, (u32)self->selected);
        sly1_port_trace_value(0x1f4c0103u, (u32)self->state);
        sly1_port_trace_value(0x1f4c0104u, *(u32*)&self->fadeTime);
        sly1_port_trace_value(0x1f4c0105u, *(u32*)&g_gameClock.time);
        sly1_port_trace_value(0x1f4c0106u, (u32)self->count);
    }
"""
            if adapted.count(update_signature) != 1:
                raise RuntimeError("expected subgroup update implementation")
            adapted = adapted.replace(update_signature, update_trace, 1)
            timer_call = """            if (t < g_gameClock.time) {
                StartObjectSubGroupFade(self);"""
            timer_trace = """            if (t < g_gameClock.time) {
                sly1_port_trace_value(0x1f4c0107u, *(u32*)&t);
                sly1_port_trace_value(0x1f4c0108u, *(u32*)&g_gameClock.time);
                StartObjectSubGroupFade(self);"""
            if adapted.count(timer_call) != 1:
                raise RuntimeError("expected subgroup finish timer comparison")
            adapted = adapted.replace(timer_call, timer_trace, 1)

            fade_signature = "void StartObjectSubGroupFade(ObjectSubGroup* self)\n{"
            fade_trace = fade_signature + """
    sly1_port_trace_value(0x1f4fa001u, (u32)self);
    sly1_port_trace_value(0x1f4fa002u, self != 0 ? (u32)self->f2FC : 0);
    sly1_port_trace_value(0x1f4fa003u, self != 0 ? *(u32*)&self->f318 : 0);
    sly1_port_trace_value(0x1f4fa004u, self != 0 ? (u32)self->f2E4 : 0);
"""
            if adapted.count(fade_signature) != 1:
                raise RuntimeError("expected subgroup fade implementation")
            adapted = adapted.replace(fade_signature, fade_trace, 1)
            fade_call = "    StartScreenFade(&g_screenFade, &m, self->f318);"
            fade_call_trace = fade_call + """
    sly1_port_trace_value(0x1f4fa005u, (u32)g_screenFade.mode);
    sly1_port_trace_value(0x1f4fa006u, (u32)g_screenFade.style);
    sly1_port_trace_value(0x1f4fa007u, *(u32*)&g_screenFade.modeStartTime);
"""
            latest_fade_call = "    StartScreenFade(&g_screenFade, &m, self->exitTarget);"
            latest_fade_call_trace = latest_fade_call + """
    sly1_port_trace_value(0x1f4fa005u, (u32)g_screenFade.mode);
    sly1_port_trace_value(0x1f4fa006u, (u32)g_screenFade.style);
    sly1_port_trace_value(0x1f4fa007u, *(u32*)&g_screenFade.modeStartTime);
"""
            if adapted.count(fade_call) == 1:
                adapted = adapted.replace(fade_call, fade_call_trace, 1)
            elif adapted.count(latest_fade_call) == 1:
                adapted = adapted.replace(latest_fade_call, latest_fade_call_trace, 1)
            else:
                raise RuntimeError("expected subgroup fade request")
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
            current_needle = (
                "                CreateAnimationPlayerWithoutPresetTargets(e->anim, "
                "(BaseObject*)self->object, self->playerFlags, &self->player, fa, fb);")
            owner_expr = "self->owner"
            if adapted.count(current_needle) == 1:
                needle = current_needle
                owner_expr = "self->object"
            replacement = (
                '                sly1_port_trace_value(0x1b66a001u, (u32)e->anim);\n'
                '                sly1_port_trace_value(0x1b66a002u, (u32)' + owner_expr + ');\n' + needle + '\n'
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
            old_needle = (
                "void CreateAnimationPlayer(Animation* self, Animation* alt, s32 npairs, "
                "TargetBinding* pairs, s32 flags, AnimationPlayer** out, f32 fa, f32 fb)\n{\n")
            latest_needle = (
                "void CreateAnimationPlayer(Animation* self, BaseObject* alt, s32 npairs, "
                "TargetBinding* pairs, s32 flags, AnimationPlayer** out, f32 fa, f32 fb)\n{\n")
            needle = old_needle if adapted.count(old_needle) == 1 else latest_needle
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
            old_wrapper = (
                "s32 CreateAnimationPlayerWithoutPresetTargets(void* a, void* b, s32 c, s32 d)\n{\n")
            latest_wrapper = (
                "void CreateAnimationPlayerWithoutPresetTargets(Animation* self, "
                "BaseObject* alt, s32 flags, AnimationPlayer** out, f32 fa, f32 fb)\n{\n")
            needle = old_wrapper if adapted.count(old_wrapper) == 1 else latest_wrapper
            replacement = needle + (
                "    sly1_port_trace_value(0x12c7d801u, (u32)__builtin_return_address(0));\n"
                "    sly1_port_trace_value(0x12c7d802u, (u32)a);\n"
                "    sly1_port_trace_value(0x12c7d803u, (u32)b);\n"
                "    sly1_port_trace_value(0x12c7d804u, (u32)c);\n"
                "    sly1_port_trace_value(0x12c7d805u, (u32)d);\n")
            if adapted.count(needle) != 1:
                raise RuntimeError("expected CreateAnimationPlayer wrapper")
            if needle == latest_wrapper:
                replacement = needle + (
                    "    sly1_port_trace_value(0x12c7d801u, (u32)__builtin_return_address(0));\n"
                    "    sly1_port_trace_value(0x12c7d802u, (u32)self);\n"
                    "    sly1_port_trace_value(0x12c7d803u, (u32)alt);\n"
                    "    sly1_port_trace_value(0x12c7d804u, (u32)flags);\n"
                    "    sly1_port_trace_value(0x12c7d805u, (u32)out);\n")
            adapted = adapted.replace(needle, replacement, 1)
            old_returned = "void* CreateAnimationPlayerReturned(void* self, s32 key, s32 a2)\n{\n"
            latest_returned = (
                "AnimationPlayer* CreateAnimationPlayerReturned(Animation* self, "
                "BaseObject* owner, f32 fa, f32 fb, s32 flags)\n{\n")
            needle = old_returned if adapted.count(old_returned) == 1 else latest_returned
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
            if needle == latest_returned:
                replacement = needle + (
                    "    sly1_port_trace_value(0x12c7d821u, (u32)__builtin_return_address(0));\n"
                    "    sly1_port_trace_value(0x12c7d822u, (u32)self);\n"
                    "    sly1_port_trace_value(0x12c7d823u, (u32)owner);\n"
                    "    sly1_port_trace_value(0x12c7d824u, (u32)flags);\n")
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
                        'extern "C" void sly1_native_trace_startup(u32, u32, u32);\n'
                        'extern "C" void sly1_port_trace_value(u32, u32);\n')
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
                # The confirmed Continue transition enters this function and
                # stops advancing before its completion callback. Bracket the
                # key lookup, sector stream, block header, and typed world read
                # in staged host source only; never alter matching source.
                load_probes = (
                    ("    SetScreenOwner(2);\n",
                     "    SetScreenOwner(2);\n"
                     "    sly1_port_trace_value(0x1e4ea801u, (u32)self->key);\n"),
                    ("        if (key.second == 0) goto end;\n",
                     "        sly1_port_trace_value(0x1e4ea802u, (u32)key.first);\n"
                     "        sly1_port_trace_value(0x1e4ea803u, (u32)key.second);\n"
                     "        if (key.second == 0) goto end;\n"),
                    ("            if (OpenBufferedSectorStreamRange(&sub, &key) == 0) {\n",
                     "            s32 stream_open = OpenBufferedSectorStreamRange(&sub, &key);\n"
                     "            sly1_port_trace_value(0x1e4ea804u, (u32)stream_open);\n"
                     "            if (stream_open == 0) {\n"),
                    ("            ReadBufferedBlockHeader(ne, &sub);\n",
                     "            ReadBufferedBlockHeader(ne, &sub);\n"
                     "            sly1_port_trace_value(0x1e4ea805u, (u32)sub.remaining);\n"),
                    ("            g_currentWorld->desc->m0A(g_currentWorld, &sub);\n",
                     "            g_currentWorld->desc->m0A(g_currentWorld, &sub);\n"
                     "            sly1_port_trace_value(0x1e4ea806u, (u32)sub.remaining);\n"),
                    ("            CloseBufferedStream(&sub);\n",
                     "            CloseBufferedStream(&sub);\n"
                     "            sly1_port_trace_value(0x1e4ea807u, (u32)g_currentWorld);\n"),
                )
                for old_probe, new_probe in load_probes:
                    if adapted.count(old_probe) != 1:
                        raise RuntimeError("expected one world-load probe site in " +
                                           rel.as_posix())
                    adapted = adapted.replace(old_probe, new_probe, 1)
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
        elif rel.as_posix() == "main/main_loop.cpp" and (
                args.trace_functions or args.trace_main_loop):
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
                    'extern "C" void sly1_native_drive_splash_notifier();\n'
                    'extern "C" void sly1_native_trace_note_world(u32);\n'
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
                ("            QueueScreenElementDraws((ElementStack*)g_elementStack);", 4),
                ("            DrawAllScreenElements((ElementStack*)g_elementStack);", 5),
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
            world_walk = "            RunWorldFrameLists(g_currentWorld, g_camera);"
            adapted = adapted.replace(
                world_walk,
                "            sly1_native_trace_note_world((u32)g_currentWorld);\n"
                "            sly1_native_drive_splash_notifier();\n" + world_walk,
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
                "                visible_ui, text_ui, (u32)g_padRecord.connectionMode, (u32)g_padRecord.connectionStep,\n"
                "                (u32)g_padRecord.flags, (u32)g_pauseMenuElement.open,\n"
                "                (u32)g_pauseMenuElement.page, (u32)g_pauseMenuElement.mode,\n"
                "                (u32)g_screenFade.mode, stack_top, stack_top_vt,\n"
                "                stack_top_mode, stack_top_text, (u32)g_padRouting,\n"
                "                (u32)g_padSelectFlags, (u32)g_blankPadRecord.connectionMode);\n"
                "            sly1_native_trace_memory_gate((u32)g_memoryCard.state,\n"
                "                (u32)g_memoryCard.targetState, (u32)g_memoryCard.displayMode,\n"
                "                (u32)g_memoryCard.i4F44, (u32)D_002701B8, stack[1],\n"
                "                (u32)g_backgroundJob.thread, (u32)g_backgroundJob.job,\n"
                "                (u32)g_backgroundJob.result, (u32)g_backgroundJob.semaphore);\n"
                "            sly1_native_trace_memory_timing(g_gameClock.elapsed,\n"
                "                g_memoryCard.stateTime, g_memoryCard.displayModeTime);\n"
                "        }\n"
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
            declaration_patterns = (
                "StreamWord ReadBufferedU32(void* src);",
                "StreamWord ReadBufferedU32(BufferedStream* self);",
            )
            declaration = next((item for item in declaration_patterns
                                if adapted.count(item) == 1), None)
            if declaration is None or sum(adapted.count(item)
                                          for item in declaration_patterns) != 1:
                raise RuntimeError("expected one stream-word reader declaration in " +
                                   rel.as_posix())
            scalar_decl = declaration.replace("StreamWord", "s32")
            adapted = adapted.replace(declaration, scalar_decl, 1)
            local_pattern = r"StreamWord tmp = ReadBufferedU32\(([^;]+)\);"
            adapted, locals_ = re.subn(
                local_pattern, r"s32 tmp = ReadBufferedU32(\1);", adapted)
            if locals_ != 1:
                raise RuntimeError("expected one stream-word reader use in " + rel.as_posix())
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
            latest_callsite = """        ReadCappedWordList((CappedWordList*)((u8*)m + 0x130), t, self, stream);"""
            latest_callsite_new = """        sly1_native_trace_capped_list(1, (u32)t, (u32)((u8*)m + 0x130), (u32)stream);
        ReadCappedWordList((CappedWordList*)((u8*)m + 0x130), t, self, stream);"""
            if adapted.count(callsite) == 1:
                adapted = adapted.replace(callsite, callsite_new, 1)
            elif adapted.count(latest_callsite) == 1:
                adapted = adapted.replace(latest_callsite, latest_callsite_new, 1)
            else:
                raise RuntimeError("expected capped-list call site in " + rel.as_posix())
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
        if args.trace_functions and rel.as_posix() == "main/warp_menu.cpp":
            # Staging-only trace of the state-0 Continue handoff. Keep it
            # separate from the source compatibility chain above.
            adapted = ('extern "C" void sly1_port_trace_value(u32, u32);\n'
                       'extern f32 D_00261850;\nextern f32 D_00261854;\n' + adapted)
            signature = "void UpdateElementStack(ElementStack* obj)\n{"
            trace = signature + """
    if (obj->state == 0) {
        sly1_port_trace_value(0x1eea0001u, (u32)obj);
        sly1_port_trace_value(0x1eea0002u, (u32)obj->state);
        sly1_port_trace_value(0x1eea0003u, (u32)D_002701B8);
        sly1_port_trace_value(0x1eea0004u, (u32)g_memoryCard.displayMode);
        sly1_port_trace_value(0x1eea0005u, (u32)g_memoryCard.state);
        sly1_port_trace_value(0x1eea0007u, (u32)g_gameClock.running);
        sly1_port_trace_value(0x1eea0008u, *(u32*)&g_gameClock.time);
        sly1_port_trace_value(0x1eea0009u, *(u32*)&g_gameClock.elapsed);
        sly1_port_trace_value(0x1eea000Au, *(u32*)&g_gameClock.dt);
        sly1_port_trace_value(0x1eea000Bu, *(u32*)&D_00261850);
        sly1_port_trace_value(0x1eea000Cu, *(u32*)&D_00261854);
    }
"""
            if adapted.count(signature) != 1:
                raise RuntimeError("expected UpdateElementStack definition")
            adapted = adapted.replace(signature, trace, 1)
            callsite = """        if (a2) {
            StartObjectSubGroupFinish(FindActiveObjectSubGroup());"""
            current_callsite = """        if (a2) {
            StartObjectSubGroupFinish((ObjectSubGroup*)FindActiveObjectSubGroup());"""
            if adapted.count(current_callsite) == 1:
                callsite = current_callsite
            callsite_trace = """        sly1_port_trace_value(0x1eea0006u, (u32)a2);
        if (a2) {
            StartObjectSubGroupFinish((ObjectSubGroup*)FindActiveObjectSubGroup());"""
            if callsite == """        if (a2) {
            StartObjectSubGroupFinish(FindActiveObjectSubGroup());""":
                callsite_trace = """        sly1_port_trace_value(0x1eea0006u, (u32)a2);
        if (a2) {
            StartObjectSubGroupFinish(FindActiveObjectSubGroup());"""
            if adapted.count(callsite) != 1:
                raise RuntimeError("expected state-0 subgroup call site")
            adapted = adapted.replace(callsite, callsite_trace, 1)
        if rel.as_posix() == "main/warp_menu.cpp":
            # Retail passes ElementStack* in the integer argument register and
            # the scale in the EE floating-point argument register. The
            # matched one-argument ApplyStackTimeScale body reads that float
            # from $f12, so the extra pointer is harmless on the PS2 ABI. On
            # the host ABI it becomes the first argument and corrupts the
            # clock scale. Drop this unused object argument only in staged
            # host source; never alter the matching decompilation.
            if "void ApplyStackTimeScale(ElementStack* stack, f32 x)" not in adapted:
                adapted, removed_scale_receivers = re.subn(
                    r"\bApplyStackTimeScale\((?:obj|self),\s*([^;\n]+?)\);",
                    r"ApplyStackTimeScale(\1);",
                    adapted,
                )
                if removed_scale_receivers != 7:
                    raise RuntimeError(
                        "expected seven EE float-register ApplyStackTimeScale call sites, "
                        f"found {removed_scale_receivers}"
                    )
            if args.trace_model_draws:
                # Instrument only the host staging copy. ReserveVifUnpack is
                # matched decomp C; this callback lets the native renderer
                # identify which reserved payloads contain invalid data.
                adapted = ('extern "C" void sly1_native_trace_vif_reserve('
                           'u32, u32, u32, u32, u32, u32, u32);\n'
                           'extern "C" void sly1_native_trace_vif_unpack_writer('
                           'u32, u32, u32, u32, u32, u32);\n'
                           'extern "C" void sly1_native_trace_vif_mscal('
                           'u32, u32, u32);\n' + adapted)
                signature = (
                    "void ReserveVifUnpack(PacketWriter* s, s32 a, s32 size, s32 d, "
                    "s32* outSize, u8** outCursor)\n{")
                if adapted.count(signature) != 1:
                    raise RuntimeError("expected one ReserveVifUnpack definition")
                adapted = adapted.replace(
                    signature,
                    signature + "\n    const s32 trace_element_count = size;", 1)
                tail = (
                    "        if (outSize) *outSize = size;\n"
                    "    }\n"
                    "}\n\n"
                    "// Reserves a VIF unpack in the packet writer")
                traced_tail = (
                    "        if (outSize) *outSize = size;\n"
                    "    }\n"
                    "    sly1_native_trace_vif_reserve("
                    "(u32)s, (u32)a, (u32)trace_element_count, (u32)d, "
                    "outSize ? (u32)*outSize : 0u, "
                    "outCursor && *outCursor ? (u32)*outCursor : 0u, "
                    "(u32)__builtin_return_address(0));\n"
                    "}\n\n"
                    "// Reserves a VIF unpack in the packet writer")
                if adapted.count(tail) != 1:
                    raise RuntimeError("expected ReserveVifUnpack output assignment")
                adapted = adapted.replace(tail, traced_tail, 1)
                signature = (
                    "void WriteVifUnpack(PacketWriter* self, s32 a, s32 b, void* c, "
                    "s32 d, s32* out)\n{")
                if adapted.count(signature) != 1:
                    raise RuntimeError("expected one WriteVifUnpack definition")
                trace = (
                    signature + "\n    if (a == 0xC && b == 7 && d == 0x1D4) {\n"
                    "        sly1_native_trace_vif_unpack_writer("
                    "(u32)self, (u32)a, (u32)b, (u32)c, (u32)d, "
                    "(u32)__builtin_return_address(0));\n    }")
                adapted = adapted.replace(signature, trace, 1)
                for function, command in (("WriteVifMscal", 0x14),
                                          ("WriteVifMscalf", 0x15)):
                    signature = f"void {function}(PacketWriter* s, void* program)\n{{"
                    if adapted.count(signature) != 1:
                        raise RuntimeError(f"expected one {function} definition")
                    trace = (
                        signature + "\n    sly1_native_trace_vif_mscal("
                        f"(u32)program / 8u, {command}u, "
                        "(u32)__builtin_return_address(0));")
                    adapted = adapted.replace(signature, trace, 1)
        if args.trace_functions and rel.as_posix() == "main/game_clock.cpp":
            adapted = 'extern "C" void sly1_port_trace_value(u32, u32);\n' + adapted
            signature = "void SetGameClockScale(f32 x)\n{"
            trace = signature + """
    static u32 trace_count;
    if (trace_count++ < 32) {
        sly1_port_trace_value(0x142ed001u, *(u32*)&x);
        sly1_port_trace_value(0x142ed002u, (u32)__builtin_return_address(0));
    }
"""
            if adapted.count(signature) != 1:
                raise RuntimeError("expected game clock scale setter")
            adapted = adapted.replace(signature, trace, 1)
        for address in backfill_addresses:
            names = {"func_" + address.upper()}
            if address in address_names:
                names.add(address_names[address])
            for name in names:
                definition = re.compile(
                    r"(?m)^(?P<header>\s*(?:extern\s+\"C\"\s+)?"
                    r"(?:[A-Za-z_][\w:*&<>]*\s+)+\b" +
                    re.escape(name) + r"\s*\([^{};]*\)[^{};]*)\s*(?=\{)")
                adapted = definition.sub(
                    lambda match: match.group("header").replace(
                        name, "decomp_" + name, 1) + " ", adapted)
        if rel.as_posix() == "main/placed_object_model.cpp":
            # Opt-in: trace the inputs UpdateAttachmentFrames composes into the
            # final bone matrix (owner inverse a, attachment transform c, local
            # q10) and its output m50, to find where extreme values enter.
            adapted = ('extern "C" void sly1_native_trace_attachment_compose(u32,u32,u32,u32);\n') + adapted
            compose_needle = (
                "                *(Mat4A*)&el->m50 = MultiplyMatrices(*(Mat4A*)&e, *(Mat4A*)&el->q10);")
            if adapted.count(compose_needle) == 1:
                adapted = adapted.replace(compose_needle,
                    "                sly1_native_trace_attachment_compose((u32)self, (u32)el, (u32)&e, (u32)&el->q10);\n"
                    "                *(Mat4A*)&el->m50 = MultiplyMatrices(*(Mat4A*)&e, *(Mat4A*)&el->q10);", 1)
        if args.trace_model_draws and rel.as_posix() == "main/placed_object_model.cpp":
            adapted = 'extern "C" void sly1_native_trace_model_part(u32, u32, u32);\n' + adapted
            adapted = ('extern "C" void sly1_native_trace_detached_mesh_packet(u32, u32, u32, u32, u32, u32);\n' + adapted)
            adapted = ('extern "C" void sly1_native_trace_strip_reserve(u32, u32, u32, u32);\n' + adapted)
            needle = "    PartSkin* skin = self->skin;\n"
            replacement = needle + (
                "    sly1_native_trace_model_part((u32)self, (u32)g_currentWorld, (u32)g_camera);\n")
            if adapted.count(needle) != 1:
                raise RuntimeError("expected DrawModelPart request initialization")
            adapted = adapted.replace(needle, replacement, 1)
            detach = (
                "                        DetachPacketBufferCopy((PacketWriter*)&pw[n], &strip->qwc[n], &starts[n], (s32*)&strip->addr[n], 0x13);\n"
            )
            if adapted.count(detach) != 1:
                raise RuntimeError("expected strip mesh packet detach")
            adapted = adapted.replace(
                detach,
                detach +
                "                        sly1_native_trace_detached_mesh_packet((u32)mesh, (u32)mesh->kind, n, (u32)strip->qwc[n], starts[n], (u32)strip->addr[n]);\n",
                1)
            builder_signature = (
                "void BuildStripMeshPacket(MeshBuildState* self, StripMeshOwner* obj, s32 mode, StripHeader* hdr, StripMesh* mesh,\n"
                "                   Vec3* vecs, Vec3* nrms, u8 (*tints)[4], MeshUv* uvs, s32 n, VertexRefs* recs,\n"
                "                   WeightIndices* ents, Vec3* verts, Vec3* vnrms, f32* weights, PacketWriter* pkt,\n"
                "                   s32** outs)\n{"
            )
            if adapted.count(builder_signature) != 1:
                raise RuntimeError("expected BuildStripMeshPacket definition for strip reserve trace")
            builder_start = adapted.index(builder_signature)
            builder_end = adapted.index("// Builds the VIF packets of a lit strip mesh", builder_start)
            builder = adapted[builder_start:builder_end]
            strip_reserve = "        ReserveVifUnpack(pkt, 0xC, s->stride, off, &c, (u8**)&d);\n"
            if builder.count(strip_reserve) != 1:
                raise RuntimeError("expected one trailing strip ReserveVifUnpack")
            builder = builder.replace(
                strip_reserve,
                "        sly1_native_trace_strip_reserve((u32)mesh, (u32)s->stride, (u32)off, (u32)pkt);\n" +
                strip_reserve,
                1)
            adapted = adapted[:builder_start] + builder + adapted[builder_end:]
            adapted = ('extern "C" void sly1_native_trace_model_build(u32, u32, u32, u32, u32, u32);\n' + adapted)
            signature = (
                "void BuildStripMeshPacket(MeshBuildState* self, StripMeshOwner* obj, s32 mode, StripHeader* hdr, StripMesh* mesh,\n"
                "                   Vec3* vecs, Vec3* nrms, u8 (*tints)[4], MeshUv* uvs, s32 n, VertexRefs* recs,\n"
                "                   WeightIndices* ents, Vec3* verts, Vec3* vnrms, f32* weights, PacketWriter* pkt,\n"
                "                   s32** outs)\n{"
            )
            replacement = signature + (
                "\n    sly1_native_trace_model_build((u32)mesh, (u32)vecs, (u32)uvs, "
                "(u32)n, (u32)recs, (u32)hdr);"
            )
            if adapted.count(signature) != 1:
                raise RuntimeError("expected BuildStripMeshPacket definition")
            adapted = adapted.replace(signature, replacement, 1)
        if rel.as_posix() == "main/point_graph.cpp":
            # Port-only staged definition for the ASM_FUNC MeasurePointGraphEdges
            # (func_001919F8). The decomp marks it assembly-only, so hostgen emits
            # a noreturn trap; the forced level load calls it from ReadPointGraph
            # and the trap kills the game thread. The archived wave treated it as
            # a logged no-op; it only measures edge lengths for later pathfinding,
            # not world loading or rendering. Define it here in the staged C so
            # the call resolves to a real body instead of the trap. The matched
            # decompilation source is untouched.
            asm_stub = "ASM_FUNC(func_001919F8);"
            stub_body = (
                "void func_001919F8(void* graph) { (void)graph; } // port no-op: MeasurePointGraphEdges")
            if adapted.count(asm_stub) == 1:
                adapted = adapted.replace(asm_stub, stub_body, 1)
        if args.trace_model_draws and rel.as_posix() == "main/point_graph.cpp":
            adapted = (
                'extern "C" void sly1_native_trace_model_hook(u32, u32, u32, u32, u32, u32);\n'
                'extern "C" void sly1_native_trace_queue_model_parts(u32, u32, u32, u32, u32);\n'
                'extern "C" void sly1_native_trace_queue_model_result(u32, u32, u32, u32, u32, u32, u32, u32, u32);\n'
                'extern "C" void sly1_native_trace_model_cull(u32, u32, u32, u32, f32, f32, f32, f32, f32, u32);\n'
                'extern "C" void sly1_native_trace_spr_block(u32, u32, u32, u32, u32, u32, u32);\n'
                + adapted
            )
            queue_signature = "void QueueModelPartDraws(PlacedObject* self, Camera* cam)\n{"
            if adapted.count(queue_signature) != 1:
                raise RuntimeError("expected one QueueModelPartDraws definition")
            adapted = adapted.replace(
                queue_signature,
                queue_signature + (
                    "\n    sly1_native_trace_queue_model_parts((u32)self, (u32)cam, "
                    "(u32)self->drawPartCount, (u32)self->drawParts, "
                    "(u32)self->partStates);"
                ),
                1,
            )
            adapted = adapted.replace(
                queue_signature + (
                    "\n    sly1_native_trace_queue_model_parts((u32)self, (u32)cam, "
                    "(u32)self->drawPartCount, (u32)self->drawParts, "
                    "(u32)self->partStates);"
                ),
                queue_signature + (
                    "\n    sly1_native_trace_queue_model_parts((u32)self, (u32)cam, "
                    "(u32)self->drawPartCount, (u32)self->drawParts, "
                    "(u32)self->partStates);"
                    "\n    u32 sly_seen = 0, sly_mask = 0, sly_plane = 0, sly_fade = 0, sly_zero = 0, sly_queued = 0;"
                    "\n    u32 sly_cull_samples = 0;"
                ),
                1,
            )
            count_edits = [
                ("        for (s32 j = 0; j < n; j++) {\n",
                 "        for (s32 j = 0; j < n; j++) {\n            ++sly_seen;\n"),
                ("            if ((st->mask & views) != views) {\n                continue;\n",
                 "            if ((st->mask & views) != views) {\n                ++sly_mask;\n                continue;\n"),
                ("            if (signs != 0) {\n                continue;\n",
                 "            if (signs != 0) {\n                ++sly_plane;\n                continue;\n"),
                ("            if (!ComputeCameraDistanceFade(cam, d, &rec.fade, radius, part->nearFade)) {\n                continue;\n",
                 "            if (!ComputeCameraDistanceFade(cam, d, &rec.fade, radius, part->nearFade)) {\n                ++sly_fade;\n                continue;\n"),
                ("            if (rec.fade <= 0.0f) {\n                continue;\n",
                 "            if (rec.fade <= 0.0f) {\n                ++sly_zero;\n                continue;\n"),
                ("            QueueDeferredRequest((DeferredRequest*)&rec);\n",
                 "            ++sly_queued;\n            QueueDeferredRequest((DeferredRequest*)&rec);\n"),
                ("        i = next;\n        n = m;\n    }\n}\n",
                 "        i = next;\n        n = m;\n    }\n    sly1_native_trace_queue_model_result((u32)self, views, sly_seen, sly_mask, sly_plane, sly_fade, sly_zero, sly_queued, (u32)self->drawPartCount);\n}\n"),
            ]
            for old_text, new_text in count_edits:
                if adapted.count(old_text) != 1:
                    raise RuntimeError("expected one QueueModelPartDraws counter site: " + old_text[:64])
                adapted = adapted.replace(old_text, new_text, 1)
            block_lines = (
                "        ModelPart* parts = (ModelPart*)SprBlock::Of(0);\n"
                "        ModelPartState* states = (ModelPartState*)SprBlock::Of(1);\n"
            )
            if adapted.count(block_lines) != 1:
                raise RuntimeError("expected one scratchpad block pointer pair")
            adapted = adapted.replace(
                block_lines,
                block_lines + (
                    "        sly1_native_trace_spr_block((u32)self, "
                    "(u32)((u8*)self->drawParts + i * 0x70), (u32)parts, "
                    "(u32)((u8*)self->partStates + i * 0x28), (u32)states, "
                    "(u32)n, (u32)g_sprTransferIndex);\n"
                ),
                1,
            )
            signs_line = "            u32 signs = Vu0::PlaneSigns(Vu0::Q(cam->planes[0]), Vu0::Q(cam->planes[1]), Vu0::Q(cam->planes[2]), d, ones, Vu0::Scalar(radius)) ^ 0xF0;\n"
            if adapted.count(signs_line) != 1:
                raise RuntimeError("expected one QueueModelPartDraws plane-sign calculation")
            adapted = adapted.replace(
                signs_line,
                signs_line + (
                    "            if (sly_cull_samples++ < 32u) sly1_native_trace_model_cull("
                    "(u32)self, (u32)(i + j), st->mask, views, part->pos.x, "
                    "part->pos.y, part->pos.z, radius, part->nearFade, signs);\n"
                ),
                1,
            )
            needle = "            if (part->hook != 0) {\n"
            replacement = (
                "                sly1_native_trace_model_hook((u32)self, (u32)(i + j), "
                "(u32)(self->drawParts + (i + j) * 0x70), (u32)part, "
                "(u32)part->hook, (u32)part->kind);\n" + needle)
            if adapted.count(needle) != 1:
                raise RuntimeError("expected QueueModelPartDraws part hook")
            adapted = adapted.replace(needle, replacement, 1)
        if args.native_sly_world_renderer and rel.as_posix() == "main/placed_object_model.cpp":
            adapted = (
                'extern "C" void sly1_native_capture_model_mesh(u32, u32, u32, u32, u32, u32, u32, u32, u32, u32, u32, u32, u32, u32);\n'
                'extern "C" void sly1_native_bind_model_mesh(u32, u32);\n'
                'extern "C" void sly1_native_trace_mesh_sources(u32, u32);\n'
                'extern "C" void sly1_native_submit_model_part(u32, u32, u32);\n' + adapted
            )
            signature = (
                "void BuildStripMeshPacket(MeshBuildState* self, StripMeshOwner* obj, s32 mode, StripHeader* hdr, StripMesh* mesh,\n"
                "                   Vec3* vecs, Vec3* nrms, u8 (*tints)[4], MeshUv* uvs, s32 n, VertexRefs* recs,\n"
                "                   WeightIndices* ents, Vec3* verts, Vec3* vnrms, f32* weights, PacketWriter* pkt,\n"
                "                   s32** outs)\n{"
            )
            replacement = signature
            if adapted.count(signature) != 1:
                raise RuntimeError("expected BuildStripMeshPacket definition for native renderer")
            adapted = adapted.replace(signature, replacement, 1)
            source_hook = (
                "    if (node == 0 && mesh->hook != 0) {\n"
                "        node = ((CursorHook*)mesh->hook)->vt[5]((CursorHook*)mesh->hook, mesh);\n"
                "    }\n"
            )
            if adapted.count(source_hook) != 2:
                raise RuntimeError("expected both strip builders to resolve their animation cursor")
            adapted = adapted.replace(
                source_hook,
                source_hook +
                "    sly1_native_trace_mesh_sources((u32)mesh, (u32)node);\n"
                "    sly1_native_capture_model_mesh((u32)mesh, (u32)vecs, (u32)nrms, (u32)tints, "
                "(u32)uvs, (u32)n, (u32)recs, (u32)node, (u32)hdr, "
                "(u32)ents, (u32)verts, (u32)self->nweights, (u32)weights, "
                "(u32)hdr->count);\n",
            )
            bind_needle = (
                "                // address arithmetic over a variable index\n"
                "                strip->unpack = (StripUnpack*)((u8*)strip->unpack + (strip->addr[0] - starts[0]));\n"
            )
            if adapted.count(bind_needle) != 1:
                raise RuntimeError("expected strip packet address bind point")
            adapted = adapted.replace(
                bind_needle,
                bind_needle +
                "                sly1_native_bind_model_mesh((u32)strip, (u32)strip->addr[0]);\n",
                1,
            )
            needle = "    b.m = self->m;\n"
            replacement = needle + (
                "    sly1_native_submit_model_part((u32)self, (u32)g_currentWorld, (u32)g_camera);\n"
            )
            if adapted.count(needle) != 1:
                raise RuntimeError("expected DrawModelPart matrix setup for native renderer")
            adapted = adapted.replace(needle, replacement, 1)
            signature = (
                "void DrawModelPart(PartDrawRequest* self)\n{"
            )
            trace_scope = (
                'extern "C" void sly1_native_trace_model_part_packet_range(\n'
                '    u32, u32, u32, u32, u32, u32, u32, u32, u32);\n'
                'struct SlyNativeModelPartPacketRangeTrace {\n'
                '    u32 request, part, begin, meshes, strips, flags, lit, skinned;\n'
                '    SlyNativeModelPartPacketRangeTrace(PartDrawRequest* r, DrawnModelPart* p)\n'
                '        : request((u32)r), part((u32)p), begin(g_vifPacketWriter.curU),\n'
                '          meshes((u32)p->nmeshes), strips((u32)p->nstrips),\n'
                '          flags(p->flags), lit((u32)p->lit), skinned((u32)p->skinned) {}\n'
                '    ~SlyNativeModelPartPacketRangeTrace() {\n'
                '        sly1_native_trace_model_part_packet_range(\n'
                '            request, part, begin, g_vifPacketWriter.curU, meshes, strips,\n'
                '            flags, lit, skinned);\n'
                '    }\n'
                '};\n' + signature
            )
            if adapted.count(signature) != 1:
                raise RuntimeError("expected DrawModelPart definition for packet ranges")
            adapted = adapted.replace(signature, trace_scope, 1)
            needle = "    b.m = self->m;\n"
            replacement = needle + (
                "    SlyNativeModelPartPacketRangeTrace packet_range(self, part);\n"
            )
            if adapted.count(needle) != 1:
                raise RuntimeError("expected DrawModelPart packet-range scope point")
            adapted = adapted.replace(needle, replacement, 1)
            item_trace_type = (
                'extern "C" void sly1_native_trace_model_part_item_packet_range(\n'
                '    u32, u32, u32, u32, u32, u32, u32, u32, u32);\n'
                'struct SlyNativeModelPartItemPacketRangeTrace {\n'
                '    u32 request, part, begin, index, kind, count, flags;\n'
                '    SlyNativeModelPartItemPacketRangeTrace(PartDrawRequest* r, DrawnModelPart* p, u32 i, u32 k)\n'
                '        : request((u32)r), part((u32)p), begin(g_vifPacketWriter.curU),\n'
                '          index(i), kind(k), count(k == 0 ? (u32)p->nmeshes : (u32)p->nstrips),\n'
                '          flags(p->flags) {}\n'
                '    ~SlyNativeModelPartItemPacketRangeTrace() {\n'
                '        sly1_native_trace_model_part_item_packet_range(\n'
                '            request, part, begin, g_vifPacketWriter.curU, index, kind, count, flags, 0);\n'
                '    }\n'
                '};\n'
            )
            item_type_needle = 'struct SlyNativeModelPartPacketRangeTrace {\n'
            if adapted.count(item_type_needle) != 1:
                raise RuntimeError("expected model-part packet range helper")
            adapted = adapted.replace(item_type_needle, item_trace_type + item_type_needle, 1)
            mesh_needle = "        e = &part->meshes[i];\n"
            if adapted.count(mesh_needle) != 1:
                raise RuntimeError("expected DrawModelPart mesh loop")
            adapted = adapted.replace(
                mesh_needle,
                mesh_needle + "        SlyNativeModelPartItemPacketRangeTrace item_range(self, part, i, 0);\n",
                1,
            )
            strip_needle = (
                "        PartStrip* s = &part->strips[t];\n"
            )
            if adapted.count(strip_needle) != 1:
                raise RuntimeError("expected DrawModelPart strip loop")
            adapted = adapted.replace(
                strip_needle,
                strip_needle + "        SlyNativeModelPartItemPacketRangeTrace item_range(self, part, t, 1);\n",
                1,
            )
        if args.trace_world_draw_lists and rel.as_posix() == "main/group_marker.cpp":
            adapted = ('extern "C" void sly1_native_trace_world_draw_lists(u32, u32, u32, u32, u32, u32);\n' + adapted)
            needle = "    ListFrame f1;\n    ListFrame f2;\n    ListFrame f3;\n"
            replacement = needle + (
                "    sly1_native_trace_world_draw_lists((u32)self, "
                "(u32)self->list60.head, (u32)self->list78.head, "
                "(u32)self->list60.offset, (u32)self->list78.offset, "
                "(u32)g_currentWorld);\n")
            if adapted.count(needle) != 1:
                raise RuntimeError("expected RunWorldFrameLists setup")
            adapted = adapted.replace(needle, replacement, 1)
        if args.trace_deferred_requests and rel.as_posix() == "main/group_marker.cpp":
            adapted = ('extern "C" void sly1_native_trace_deferred_requests(u32, u32, u32, u32, u32);\n' + adapted)
            signature = "void SubmitDeferredRequests(World* world, void* camera)\n{"
            if adapted.count(signature) != 1:
                raise RuntimeError("expected SubmitDeferredRequests definition")
            adapted = adapted.replace(
                signature,
                signature + "\n    void DrawPlacedObjectParts(DeferredRequest*);", 1)
            needle = "    s32 count = BucketDeferredRequests(n, g_deferredRequests, (void**)list);\n"
            replacement = needle + (
                "    sly1_native_trace_deferred_requests((u32)world, (u32)n, "
                "(u32)count, (u32)list, (u32)DrawPlacedObjectParts);\n")
            if adapted.count(needle) != 1:
                raise RuntimeError("expected SubmitDeferredRequests bucket")
            adapted = adapted.replace(needle, replacement, 1)
        if args.trace_deferred_requests and rel.as_posix() == "main/active_set_solver.cpp":
            adapted = ('extern "C" void sly1_native_trace_draw_object(u32, u32, u32, u32);\n' + adapted)
            needle = "    TextureRenderObject* s1 = (TextureRenderObject*)self->arg;\n"
            replacement = needle + (
                "    sly1_native_trace_draw_object((u32)g_currentWorld, (u32)s1, "
                "(u32)s1->drawPartCount, (u32)s1->drawParts);\n")
            if adapted.count(needle) != 1:
                raise RuntimeError("expected DrawPlacedObjectParts object load")
            adapted = adapted.replace(needle, replacement, 1)
        if args.trace_main_loop and rel.as_posix() == "main/world.cpp":
            adapted = ('extern "C" void sly1_native_trace_camera(u32, u32, u32);\n' + adapted)
            needle = "    g_camera = (Camera*)CreateTypedRecord(0x6D, self, 0, 4, -1);\n"
            replacement = needle + "    sly1_native_trace_camera(10, (u32)g_camera, (u32)self);\n"
            if adapted.count(needle) != 1:
                raise RuntimeError("expected LoadWorld camera creation")
            adapted = adapted.replace(needle, replacement, 1)
            needle = "    UpdateCameraLocation(g_camera);\n    UpdateWorldContacts(self, 0);"
            replacement = (
                "    UpdateCameraLocation(g_camera);\n"
                "    sly1_native_trace_camera(11, (u32)g_camera, (u32)self);\n"
                "    UpdateWorldContacts(self, 0);")
            if adapted.count(needle) != 1:
                raise RuntimeError("expected LoadWorld camera placement")
            adapted = adapted.replace(needle, replacement, 1)
        if args.trace_main_loop and rel.as_posix() == "main/main_loop.cpp":
            hook = 'extern "C" void sly1_native_trace_camera(u32, u32, u32);\n'
            adapted = hook + adapted
            needle = "            UpdateCameraLocation(g_camera);\n"
            replacement = needle + "            sly1_native_trace_camera(20, (u32)g_camera, (u32)g_currentWorld);\n"
            if adapted.count(needle) != 1:
                raise RuntimeError("expected main-loop camera location update")
            adapted = adapted.replace(needle, replacement, 1)
            needle = "                g_currentWorld->desc->m11(g_currentWorld, g_gameClock.dt);\n"
            replacement = needle + (
                "            sly1_native_trace_camera(21, (u32)g_camera, "
                "(u32)g_currentWorld);\n")
            if adapted.count(needle) != 1:
                raise RuntimeError("expected main-loop world update")
            adapted = adapted.replace(needle, replacement, 1)
        if args.trace_placed_object_load and rel.as_posix() == "main/placed_object.cpp":
            declaration = 'extern "C" void sly1_native_trace_placed_object_load(u32, u32, u32);\n'
            if declaration not in adapted:
                adapted = declaration + adapted
            needle = "    ReadBufferedRotation3((BufferedStream*)r, (u8*)&self->rot);"
            replacement = (
                "    sly1_native_trace_placed_object_load((u32)self, (u32)r, 0);\n"
                + needle + "\n"
                "    sly1_native_trace_placed_object_load((u32)self, (u32)r, 1);")
            if adapted.count(needle) != 1:
                raise RuntimeError("expected LoadPlacedObject rotation read")
            adapted = adapted.replace(needle, replacement, 1)
            probes = (
                ("    ReadSlotObjects(self->world, self, r);",
                 "    ReadSlotObjects(self->world, self, r);\n"
                 "    sly1_native_trace_placed_object_load((u32)self, 0, 2);"),
                ("    ListFrame frame;\n    ResolveFollowRecordObjects(self);",
                 "    ListFrame frame;\n"
                 "    sly1_native_trace_placed_object_load((u32)self, 0, 3);\n"
                 "    ResolveFollowRecordObjects(self);"),
                ("        }\n    }\n}\n\n// Start-up of the object once its world is loaded:",
                 "        }\n    }\n"
                 "    sly1_native_trace_placed_object_load((u32)self, 0, 4);\n"
                 "}\n\n// Start-up of the object once its world is loaded:"),
                ("    StartKeyNode* k;\n    BaseObject* s0;",
                 "    StartKeyNode* k;\n"
                 "    sly1_native_trace_placed_object_load((u32)self, 0, 5);\n"
                 "    BaseObject* s0;"),
                ("    *(sptr*)&g_activeListFrames = fr.prev;\n}\n\n// Per-frame update",
                 "    sly1_native_trace_placed_object_load((u32)self, 0, 6);\n"
                 "    *(sptr*)&g_activeListFrames = fr.prev;\n}\n\n// Per-frame update"),
                ("void UpdatePlacedObject(PlacedObject* self, f32 f)\n{\n"
                 "    UpdateAttachments((AnimatorOwner*)self->sub22C, self, f);",
                 "void UpdatePlacedObject(PlacedObject* self, f32 f)\n{\n"
                 "    sly1_native_trace_placed_object_load((u32)self, 0, 11);\n"
                 "    UpdateAttachments((AnimatorOwner*)self->sub22C, self, f);"),
                ("    TriggerScriptEvent(self, 1, 1, args);\n}",
                 "    TriggerScriptEvent(self, 1, 1, args);\n"
                 "    sly1_native_trace_placed_object_load((u32)self, 0, 12);\n}"),
                ("void ComputePlacedObjectWorldFrame(PlacedObject* self)\n{\n"
                 "    Rec2195_224* rec = self->linkRecord;",
                 "void ComputePlacedObjectWorldFrame(PlacedObject* self)\n{\n"
                 "    sly1_native_trace_placed_object_load((u32)self, 0, 9);\n"
                 "    Rec2195_224* rec = self->linkRecord;"),
                ("    Rec2195_224* last = self->linkRecord;\n"
                 "    if (last != 0 && (last->flags & 0x8020) == 0x8020) {",
                 "    sly1_native_trace_placed_object_load((u32)self, 0, 10);\n"
                 "    Rec2195_224* last = self->linkRecord;\n"
                 "    if (last != 0 && (last->flags & 0x8020) == 0x8020) {"),
                ("void MovePlacedObject(PlacedObject* self, s32 flag, f32 dt)\n{\n"
                 "    Attachment* pa = self->positionAttachment;",
                 "void MovePlacedObject(PlacedObject* self, s32 flag, f32 dt)\n{\n"
                 "    sly1_native_trace_placed_object_load((u32)self, 0, 7);\n"
                 "    Attachment* pa = self->positionAttachment;"),
                ("    }\n    g_activeListFrames = (ListFrame*)it.prev;\n}\n\ntypedef void (*PositionAtFn)",
                 "    }\n"
                 "    sly1_native_trace_placed_object_load((u32)self, 0, 8);\n"
                 "    g_activeListFrames = (ListFrame*)it.prev;\n}\n\n"
                 "typedef void (*PositionAtFn)"),
            )
            for needle, replacement in probes:
                if adapted.count(needle) != 1:
                    raise RuntimeError("expected placed-object pose trace site: " + needle[:80])
                adapted = adapted.replace(needle, replacement, 1)
        if args.trace_placed_object_load and rel.as_posix() == "main/pose_source.cpp":
            declaration = (
                'extern "C" void sly1_native_trace_pose_rotation('
                'u32, u32, u32, u32, u32, float, float, float);\n')
            if declaration not in adapted:
                adapted = declaration + adapted
            probes = (
                ("    self->vt->getRot(self, 0.0f, &A, &B);",
                 "    self->vt->getRot(self, 0.0f, &A, &B);\n"
                 "    sly1_native_trace_pose_rotation((u32)owner, (u32)self, 0, "
                 "(u32)&A, (u32)&B, dt, now, 0.0f);"),
                ("        MultiplyMatrices3x3(&owner->rot, &A, &m);\n"
                 "        MatrixToAxisAngle(&m, &angle, &axis);",
                 "        MultiplyMatrices3x3(&owner->rot, &A, &m);\n"
                 "        MatrixToAxisAngle(&m, &angle, &axis);\n"
                 "        sly1_native_trace_pose_rotation((u32)owner, (u32)self, 1, "
                 "(u32)&m, (u32)&axis, angle, 0.0f, 0.0f);"),
                ("        f32 reach = ApproachValueWithRamp(0.0f, angle, dt, (RampProfile*)&prof, &rate);",
                 "        f32 reach = ApproachValueWithRamp(0.0f, angle, dt, (RampProfile*)&prof, &rate);\n"
                 "        sly1_native_trace_pose_rotation((u32)owner, (u32)self, 2, "
                 "(u32)&m, (u32)&axis, angle, reach, rate);"),
            )
            for needle, replacement in probes:
                if adapted.count(needle) != 1:
                    raise RuntimeError("expected pose-source rotation trace site: " + needle[:80])
                adapted = adapted.replace(needle, replacement, 1)
        if rel.as_posix() == "main/pose_source.cpp":
            # The attachment vtable ABI passes time before the output pointers.
            # These two track callbacks were decompiled with time last and are
            # cast into those slots. That cast works in the retail EE calling
            # convention, but shifts the stack arguments in the native i386
            # host build. Reorder only the staged definitions so the host
            # callback signatures match AttachGetPos/AttachGetRot; keep the
            # matched decompilation source untouched.
            callback_abis = (
                ("void GetTrackPosition(TrackFollowAttachment* self, Vec4* a1, Vec4* a2, f32 f)",
                 "void GetTrackPosition(TrackFollowAttachment* self, f32 f, Vec4* a1, Vec4* a2)"),
                ("void GetTrackRotation(TrackFollowAttachment* self, Mat3* a1, Vec4* a2, f32 f)",
                 "void GetTrackRotation(TrackFollowAttachment* self, f32 f, Mat3* a1, Vec4* a2)"),
            )
            for old, new in callback_abis:
                if adapted.count(old) != 1:
                    raise RuntimeError("expected track callback host ABI site: " + old)
                adapted = adapted.replace(old, new, 1)
        # Matching units declare AddGifRegisterWrite with inconsistent host
        # types. The retail ABI is (writer, 32-bit register, 64-bit value);
        # normalize every staged declaration and definition to that layout.
        # Without this, a u64 register shifts the value by one stack word on
        # i386 and GS writes such as SCISSOR_1 become corrupt.
        adapted = normalize_gif_register_writer_abi(adapted)
        if rel.as_posix() == "main/placed_object.cpp":
            # Opt-in trace of QueuePlacedObjectModelParts: count objects and
            # parts entering the main model draw path (and how many are culled).
            adapted = ('extern "C" void sly1_native_trace_model_parts_enter(u32, u32, u32);\n'
                       'extern "C" void sly1_native_trace_model_draw_result(u32, u32);\n') + adapted
            qneedle = ("void QueuePlacedObjectModelParts(PlacedObject* self, Camera* cam, Quad16* src)\n{\n"
                       "    ModelPartRequest rec;\n    ModelPartState* state = 0;\n    s32 drawn = 0;")
            qrepl = ("void QueuePlacedObjectModelParts(PlacedObject* self, Camera* cam, Quad16* src)\n{\n"
                     "    ModelPartRequest rec;\n    ModelPartState* state = 0;\n    s32 drawn = 0;\n"
                     "    sly1_native_trace_model_parts_enter((u32)self, (u32)self->drawPartCount, (u32)g_currentWorld);")
            if adapted.count(qneedle) == 1:
                adapted = adapted.replace(qneedle, qrepl, 1)
            # Log parts drawn just before the function returns (find the tail).
            tail = "    if (drawn != 0) {\n        self->desc02()->m27(self, src);"
            if adapted.count(tail) == 1:
                adapted = adapted.replace(tail,
                    "    sly1_native_trace_model_draw_result((u32)self, (u32)drawn);\n" + tail, 1)
        if rel.as_posix() == "main/placed_object.cpp":
            # The final Vec4 aggregate writes compile to aligned SSE movaps on
            # host32, but retail callers can pass a less-than-16-byte-aligned
            # Vec4* (the forced-level player path faulted with #GP on such a
            # pointer). Use unaligned-safe copies; semantics are identical.
            tneedle = ("    if (velSum != 0) {\n        *velSum = V;\n    }\n"
                       "    if (spinSum != 0) {\n        *spinSum = W;\n    }")
            trepl = ("    if (velSum != 0) { __builtin_memcpy(velSum, &V, sizeof(V)); }\n"
                     "    if (spinSum != 0) { __builtin_memcpy(spinSum, &W, sizeof(W)); }")
            if adapted.count(tneedle) == 1:
                adapted = adapted.replace(tneedle, trepl, 1)
        if rel.as_posix() == "main/placed_object_model.cpp":
            # Opt-in: dump the authoritative bone matrix the retail VIF unpack
            # sends (skin->bones[k].m), to confirm whether it is corrupt at the
            # source regardless of the host skinning adapter's address math.
            adapted = ('extern "C" void sly1_native_trace_bone_unpack(u32,u32);\n') + adapted
            bone_needle = (
                "                VifPacketWriter::UnpackAt(&g_vifPacketWriter, &skin->bones[e->bones[k]].m, 0x40, 0x6C04C000, addr);")
            if adapted.count(bone_needle) == 1:
                adapted = adapted.replace(bone_needle,
                    "                sly1_native_trace_bone_unpack((u32)skin, (u32)&skin->bones[e->bones[k]].m);\n" + bone_needle, 1)
        if rel.as_posix() == "main/placed_object.cpp":
            # Opt-in: trace ComputePlacedObjectWorldFrame's rotation inputs and
            # output to find where the ~1e6 bone rotation rows originate.
            adapted = ('extern "C" void sly1_native_trace_world_frame(u32,u32,u32,u32,u32);\n') + adapted
            wf_needle = "    u32 kind = self->bits24;\n"
            if adapted.count(wf_needle) == 1:
                adapted = adapted.replace(wf_needle,
                    "    sly1_native_trace_world_frame((u32)self, (u32)&self->rot, (u32)&self->m110, (u32)self->parent, (u32)&self->pos);\n" + wf_needle, 1)
            # Opt-in: trace the unscaleByParent division that could blow up the
            # attachment rotations if the parent matrix diagonal is near zero.
            adapted = ('extern "C" void sly1_native_trace_unscale(u32,u32,f32,f32,f32);\n') + adapted
            unscale_needle = (
                "                    m.r[0].x = 1.0f / m.r[0].x;\n"
                "                    m.r[1].y = 1.0f / m.r[1].y;\n"
                "                    m.r[2].z = 1.0f / m.r[2].z;")
            if adapted.count(unscale_needle) == 1:
                adapted = adapted.replace(unscale_needle,
                    "                    sly1_native_trace_unscale((u32)self, (u32)parent, m.r[0].x, m.r[1].y, m.r[2].z);\n" + unscale_needle, 1)
            # This matched non-void wrapper calls a void function and falls
            # through. GCC -O2 omits its epilogue, so host execution enters the
            # next retail function body with the previous arguments still on
            # the stack. Keep this host ABI adaptation in the generated copy;
            # the decompilation source remains unchanged.
            needle = "    CreateAnimationPlayer(a, b, 0, 0, c, d);\n}"
            replacement = "    CreateAnimationPlayer(a, b, 0, 0, c, d);\n    return 0;\n}"
            latest_wrapper = (
                "void CreateAnimationPlayerWithoutPresetTargets(Animation* self, "
                "BaseObject* alt, s32 flags, AnimationPlayer** out, f32 fa, f32 fb)"
            )
            if adapted.count(needle) == 1:
                adapted = adapted.replace(needle, replacement, 1)
            elif (adapted.count(latest_wrapper) < 1 or
                  adapted.count("CreateAnimationPlayer(self, alt, 0, 0, flags, out, fa, fb);") != 1):
                raise RuntimeError("expected host32 animation-player wrapper fallthrough")
        # The latest decomp consolidated the former Vec02/Mat02 aliases into
        # Vec4/Mat3. A few retained declarations still use the retired names;
        # normalize only when the current source unit already uses the new
        # canonical type, keeping older source revisions buildable as well.
        if "Vec4" in adapted:
            adapted = re.sub(r"\bVec02\b", "Vec4", adapted)
        if "Mat3" in adapted:
            adapted = re.sub(r"\bMat02\b", "Mat3", adapted)
        adapted = emit_text_layout_setters_for_host(adapted, rel.as_posix())
        adapted = normalize_active_set_solver_abi(adapted, rel.as_posix())
        adapted = normalize_uncached_ee_cpu_pointers(adapted, rel.as_posix())
        if rel.as_posix() == "main/main_loop.cpp":
            # The console's vblank paces RunMainLoop. SDL drives that display
            # cadence on another host thread, so pace the guest loop too; the
            # old host wait was incorrectly injected only by trace builds.
            wait_hook = 'extern "C" void sly1_native_host32_wait_frame(void);\n'
            if wait_hook not in adapted:
                adapted = wait_hook + adapted
            frame_end = "        g_frameCount++;"
            paced_frame_end = frame_end + "\n        sly1_native_host32_wait_frame();"
            if adapted.count(frame_end) != 1:
                raise RuntimeError("expected one RunMainLoop frame boundary")
            adapted = adapted.replace(frame_end, paced_frame_end, 1)
        if rel.as_posix() == "main/on_off_object.cpp":
            # Opt-in forced-level boot for the SDL build: the port backfill's
            # sly1_native_boot_level_key shortcut is not linked here (decomp C
            # func_001E4EA8 wins). Reproduce it at the top of RunPendingWorldLoad:
            # once, if a level key was requested, find the catalog entry whose
            # +0x20 word is that key and queue it for the retail loader.
            boot_hook = (
                'extern "C" u32 sly1_native_boot_level_key_for_sdl(void);\n'
                'extern "C" u32 sly1_keyed_entries_for_sdl(void);\n')
            adapted = boot_hook + adapted
            needle = ("void RunPendingWorldLoad(WorldLoadRequest* self)\n{\n"
                      "    KeyId key;\n    WorldKeyBlock blob;")
            replacement = (
                "void RunPendingWorldLoad(WorldLoadRequest* self)\n{\n"
                "    KeyId key;\n    WorldKeyBlock blob;\n"
                "    if (self->key == 0) {\n"
                "        const u32 wanted = sly1_native_boot_level_key_for_sdl();\n"
                "        const u32 table = sly1_keyed_entries_for_sdl();\n"
                "        if (wanted != 0 && table != 0) {\n"
                "            for (u32 bi = 0; bi < 46u; ++bi) {\n"
                "                u8* e = (u8*)table + bi * 0x2Cu;\n"
                "                if (*(u32*)(e + 0x20u) == wanted) {\n"
                "                    *(WorldKeyBlock**)&self->key = (WorldKeyBlock*)e;\n"
                "                    break;\n"
                "                }\n"
                "            }\n"
                "        }\n"
                "    }")
            if adapted.count(needle) == 1:
                adapted = adapted.replace(needle, replacement, 1)
            # Opt-in stage markers inside RunPendingWorldLoad to find where the
            # forced load stalls (game thread stuck at world=0).
            adapted = ('extern "C" void sly1_native_world_load_stage(u32);\n') + adapted
            markers = (
                ("            InitLoadProgress(&o30, &w60, &w70, &w80, &w90);", 1),
                ("            if (OpenBufferedSectorStreamRange(&sub, &key) == 0) {", 2),
                ("            ResetWorldSubsystems(g_currentWorld);", 3),
                ("            ReadBufferedBlockHeader(ne, &sub);", 4),
                ("            g_currentWorld = CreateTypedRecord(0x6C, 0, 0, 3, -1);", 5),
                ("            g_currentWorld->desc->m0A(g_currentWorld, &sub);", 6),
                ("            CloseBufferedStream(&sub);", 7),
            )
            for mtext, stage in markers:
                if adapted.count(mtext) == 1:
                    adapted = adapted.replace(
                        mtext,
                        "            sly1_native_world_load_stage(" + str(stage) + ");\n" + mtext, 1)
        if rel.as_posix() == "main/commentary_best_time.cpp":
            # Port-only guard: the forced-level boot can run this caption init
            # while RunPendingWorldLoad has g_currentWorld == 0. Retail never
            # triggers it then; guard the dereferences to avoid the null crash.
            needle = ("void InitCommentaryPromptElement(SprintCaption* self)\n{\n"
                      "    const LevelEntry* e;\n\n"
                      "    SetupScreenElementText(self);")
            replacement = ("void InitCommentaryPromptElement(SprintCaption* self)\n{\n"
                           "    const LevelEntry* e;\n\n"
                           "    if (g_currentWorld == 0) return;\n"
                           "    if (g_currentWorld->commentaryRecord == 0) return;\n"
                           "    SetupScreenElementText(self);")
            if adapted.count(needle) == 1:
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
