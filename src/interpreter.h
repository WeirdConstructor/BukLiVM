// Copyright (C) 2017 Weird Constructor
// For more license info refer to the the bottom of this file.

#pragma once

#include "atom.h"
#include "runtime.h"
#include "atom_printer.h"
#include "buklivm.h"

//---------------------------------------------------------------------------

class BukaLISPModule;

//---------------------------------------------------------------------------

namespace bukalisp
{
//---------------------------------------------------------------------------

class InterpreterException : public std::exception
{
    private:
        std::string m_err;
    public:
        InterpreterException(const std::string &err) : m_err(err) { }
        virtual const char *what() const noexcept { return m_err.c_str(); }
        virtual ~InterpreterException() { }
};
//---------------------------------------------------------------------------

class Interpreter
{
    private:
        Runtime        *m_rt;
        VM             *m_vm;
        GC_ROOT_MEMBER_VEC(m_env_stack);
        GC_ROOT_MEMBER_VEC(m_prim_table);
        GC_ROOT_MEMBER_MAP(m_debug_pos_map);
        GC_ROOT_MEMBER(m_compiler_func);
        AtomMap        *m_modules;
        std::string     m_debug_pos;
        std::vector<Atom::PrimFunc *> m_primitives;

        bool            m_trace;
        bool            m_force_always_gc;

        void set_debug_pos(Atom &a);

    public:
        Interpreter(Runtime *rt, VM *vm = nullptr)
            : m_rt(rt),
              m_trace(false), m_vm(vm),
              m_force_always_gc(true), m_modules(nullptr),
              GC_ROOT_MEMBER_INITALIZE_VEC(rt->m_gc, m_env_stack),
              GC_ROOT_MEMBER_INITALIZE_VEC(rt->m_gc, m_prim_table),
              GC_ROOT_MEMBER_INITALIZE_MAP(rt->m_gc, m_debug_pos_map),
              GC_ROOT_MEMBER_INITALIZE(rt->m_gc, m_compiler_func)
        {
            m_env_stack  = nullptr;
            m_prim_table = nullptr;
            init();
        }

        virtual ~Interpreter()
        {
            for (auto p : m_primitives)
                delete p;

            if (m_prim_table)
            {
                for (size_t i = 0; i < m_prim_table->m_len; i++)
                {
                    if (m_prim_table->m_data[i].m_type == T_PRIM)
                        delete m_prim_table->m_data[i].m_d.func;
                }
            }
        }

        void print_primitive_table();

        void init();

        void set_trace(bool e) { m_trace = e; }
        void set_force_always_gc(bool e) { m_force_always_gc = e; }

        Atom lookup(Sym *var, AtomMap *&env)
        {
            env = nullptr;

            if (m_env_stack->m_len <= 0)
                return Atom();

            size_t stk_idx1 = m_env_stack->m_len;

            while (stk_idx1 > 0)
            {
                Atom *at_env = &(m_env_stack->m_data[--stk_idx1]);
                if (!at_env)
                    return Atom();
                if (at_env->m_type != T_MAP)
                    return Atom();

                bool defined = false;
                Atom ret = at_env->m_d.map->at(Atom(T_SYM, var), defined);
                if (defined)
                {
                    env = at_env->m_d.map;
                    return ret;
                }
            }

            return Atom();
        }

        Atom eval(const std::string &input_name, const std::string &input)
        {
            GC_ROOT(m_rt->m_gc, old_debug_info);
            if (m_debug_pos_map)
                old_debug_info = Atom(T_MAP, m_debug_pos_map);

            GC_ROOT(m_rt->m_gc, prog) =
                m_rt->read(input_name, input, m_debug_pos_map);

//            std::cerr << "EVAL(" << write_atom(prog) << std::endl;
            Atom ret;
            try
            {
                if (prog.m_type == T_VEC)
                    ret = eval_begin(prog, prog.m_d.vec, 0);
                else
                    ret = eval(prog);
            }
            catch (...)
            {
                m_debug_pos_map =
                    old_debug_info.m_type == T_MAP
                    ? old_debug_info.m_d.map
                    : nullptr;
                throw;
            }

            m_debug_pos_map =
                old_debug_info.m_type == T_MAP
                ? old_debug_info.m_d.map
                : nullptr;

            return ret;
        }

        Atom get_compiler_func();
        Atom call_compiler(
            Atom prog,
            AtomMap *debug_info_map,
            AtomVec *root_env,
            const std::string &input_name = "",
            bool only_compile = false);

        Atom call_compiler(
            const std::string &code_name,
            const std::string &code,
            AtomVec *root_env,
            bool only_compile = false);

        void error(const std::string &msg)
        {
            throw InterpreterException("(@" + m_debug_pos + "): " + msg);
        }
        void error(const std::string &msg, Atom &err_atom)
        {
            error(msg + ", atom: " + write_atom(err_atom));
        }

        Atom call(Atom func, AtomVec *av, bool eval_args = false, size_t arg_offs = 0);
        Atom eval(Atom e);
        Atom eval_begin    (Atom e, AtomVec *av, size_t offs);
        Atom eval_define   (Atom e, AtomVec *av);
        Atom eval_setM     (Atom e, AtomVec *av);
        Atom eval_let      (Atom e, AtomVec *av);
        Atom eval_lambda   (Atom e, AtomVec *av);
        Atom eval_if       (Atom e, AtomVec *av);
        Atom eval_when     (Atom e, AtomVec *av);
        Atom eval_unless   (Atom e, AtomVec *av);
        Atom eval_while    (Atom e, AtomVec *av);
        Atom eval_and      (Atom e, AtomVec *av);
        Atom eval_or       (Atom e, AtomVec *av);
        Atom eval_for      (Atom e, AtomVec *av);
        Atom eval_do_each  (Atom e, AtomVec *av);
        Atom eval_dot_call (Atom e, AtomVec *av);
        Atom eval_meth_def (Atom e, AtomVec *av);
        Atom eval_field_set(Atom e, AtomVec *av);
        Atom eval_field_get(Atom e, AtomVec *av);
        Atom eval_case     (Atom e, AtomVec *av);
        Atom eval_include  (Atom e, AtomVec *av);
};
//---------------------------------------------------------------------------

}

/******************************************************************************
* Copyright (C) 2017 Weird Constructor
*
* Permission is hereby granted, free of charge, to any person obtaining
* a copy of this software and associated documentation files (the
* "Software"), to deal in the Software without restriction, including
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to
* permit persons to whom the Software is furnished to do so, subject to
* the following conditions:
*
* The above copyright notice and this permission notice shall be
* included in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
******************************************************************************/
