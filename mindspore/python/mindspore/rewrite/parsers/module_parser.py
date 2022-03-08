# Copyright 2022 Huawei Technologies Co., Ltd
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# ============================================================================
"""Parse ast.Module to SymbolTrees."""
from typing import Any
import os
import ast
import copy
import inspect
import astunparse

from mindspore import log as logger
from ..symbol_tree import SymbolTree
from ..parser import Parser
from ..parser_register import ParserRegister, reg_parser


class ClassFinder(ast.NodeVisitor):
    """Find all ast.ClassDef in input ast node."""

    def __init__(self):
        """Keep all found ast.ClassDef in self._classes"""
        self._classes: [ast.ClassDef] = []

    def visit_ClassDef(self, node: ast.ClassDef) -> Any:
        """Iterate over all nodes and save ast.ClassDef nodes."""
        self._classes.append(node)

    def find_all_classes(self, node: ast.AST) -> [ast.ClassDef]:
        """Interface of ClassFinder."""
        self.visit(node)
        return self._classes


class ModuleParser(Parser):
    """Parse ast.Module to SymbolTrees."""

    def target(self):
        """Parse target type"""
        return ast.Module

    @staticmethod
    def _find_class(ast_node: ast.Module) -> ast.ClassDef:
        """Find all ast.ClassDef in ast.Module, only support one ast.ClassDef in ast.Module now."""
        visitor = ClassFinder()
        classes = visitor.find_all_classes(ast_node)
        if not classes:
            raise RuntimeError("No class in module")
        if len(classes) > 1:
            raise RuntimeError("Multi-class in module is not supported now")
        return classes[0]

    @staticmethod
    def get_import_node(ast_root):
        """Iterate over ast_root and return all ast.Import nodes or ast.ImportFrom nodes in ast_root."""
        import_nodes = []

        class GetImportNode(ast.NodeVisitor):
            """Find all import nodes from input ast node."""
            def visit_Import(self, node: ast.Import) -> Any:
                """Iterate over all nodes and save ast.Import nodes."""
                import_nodes.append(copy.deepcopy(node))
                return node

            def visit_ImportFrom(self, node: ast.ImportFrom) -> Any:
                """Iterate over all nodes and save ast.ImportFrom nodes."""
                import_nodes.append(copy.deepcopy(node))
                return node

            def get_node(self, input_ast):
                """Interface of GetImportNode."""
                self.generic_visit(input_ast)
                return True

        get_node_handler = GetImportNode()
        get_node_handler.get_node(ast_root)
        return import_nodes

    @staticmethod
    def _add_import_to_module(module: ast.Module, origin_net):
        """Insert two groups of import nodes to ast.Module, common ones and those from class definition file."""
        module.body.insert(0, ast.Import([ast.alias(name='mindspore', asname=None)]))
        module.body.insert(1, ast.ImportFrom(module='mindspore', names=[ast.alias(name='nn', asname=None)], level=0))
        module.body.insert(2, ast.ImportFrom(module='mindspore.nn', names=[ast.alias(name='Cell', asname=None)],
                                             level=0))
        origin_net_source_code_file = inspect.getfile(type(origin_net))
        if not os.path.exists(origin_net_source_code_file):
            raise RuntimeError("File ", origin_net_source_code_file, " not exist")
        try:
            with open(origin_net_source_code_file, "r") as f:
                source_code = f.read()
                import_nodes = ModuleParser.get_import_node(ast.parse(source_code))
        except RuntimeError:
            raise RuntimeError("get import nodes error")
        if import_nodes:
            for import_index, import_node in enumerate(import_nodes):
                module.body.insert(import_index + 3, import_node)
        ast.fix_missing_locations(module)

    def process(self, stree: SymbolTree, node: ast.Module):
        """Process ast.ClassDef nodes in ast.Module."""
        ModuleParser._add_import_to_module(node, stree.get_origin_network())
        class_ast = ModuleParser._find_class(node)
        stree.set_class_ast(class_ast)
        for body in node.body:
            if isinstance(body, ast.ClassDef):
                parser: Parser = ParserRegister.instance().get_parser(ast.ClassDef)
                parser.process(stree, body)
            else:
                logger.info(f"Ignoring unsupported node({astunparse.unparse(body)}) in ast.Module.")


g_module_parser = reg_parser(ModuleParser())
