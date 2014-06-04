#include "runtime_spec_helper.h"
#include "tree_sitter/parser/stack.h"

START_TEST

enum {
    sym1 = 101,
    sym2 = 102,
    hidden_sym = 103,
};

int hidden_symbols[] = {
    [sym1] = 0,
    [sym2] = 0,
    [hidden_sym] = 1,
};

describe("stacks", [&]() {
    ts_stack stack;
    
    before_each([&]() {
        stack = ts_stack_make();
    });
    
    after_each([&]() {
        ts_stack_delete(&stack);
    });
    
    it("starts out empty", [&]() {
        AssertThat(stack.size, Equals(0));
        AssertThat(ts_stack_top_state(&stack), Equals(0));
        AssertThat(ts_stack_top_node(&stack), Equals((ts_tree *)nullptr));
    });
    
    describe("pushing a symbol", [&]() {
        ts_tree *node1;
        
        before_each([&]() {
            node1 = ts_tree_make_leaf(sym1, 5, 1);
            ts_stack_push(&stack, 5, node1);
        });
        
        after_each([&]() {
            ts_tree_release(node1);
        });
        
        it("adds the symbol to the stack", [&]() {
            AssertThat(stack.size, Equals(1));
            AssertThat(ts_stack_top_state(&stack), Equals(5));
            AssertThat(ts_stack_top_node(&stack), Equals(node1));
        });
    });
    
    describe("reducing a symbol", [&]() {
        ts_tree **nodes;
        
        before_each([&]() {
            nodes = tree_array({
                ts_tree_make_leaf(sym1, 5, 1),
                ts_tree_make_leaf(sym1, 5, 1),
                ts_tree_make_leaf(hidden_sym, 5, 1),
                ts_tree_make_leaf(sym1, 5, 1),
            });
            
            for (ts_state_id i = 0; i < 4; i++)
                ts_stack_push(&stack, 10 + i, nodes[i]);
        });
        
        after_each([&]() {
            for (ts_state_id i = 0; i < 4; i++)
                ts_tree_release(nodes[i]);
            free(nodes);
        });
        
        it("pops the given number of nodes off the stack", [&]() {
            AssertThat(stack.size, Equals(4));
            ts_stack_reduce(&stack, sym2, 3, hidden_symbols, nullptr);
            AssertThat(stack.size, Equals(1));
        });
        
        it("returns a node with the given symbol", [&]() {
            ts_tree *node = ts_stack_reduce(&stack, sym2, 3, hidden_symbols, nullptr);
            AssertThat(ts_tree_symbol(node), Equals(sym2));
        });
        
        it("makes all of the removed nodes immediate children of the new node", [&]() {
            ts_tree *expected_children[3] = {
                stack.entries[1].node,
                stack.entries[2].node,
                stack.entries[3].node,
            };
            
            ts_tree *node = ts_stack_reduce(&stack, sym2, 3, hidden_symbols, nullptr);
            size_t immediate_child_count;
            ts_tree **immediate_children = ts_tree_immediate_children(node, &immediate_child_count);

            AssertThat(immediate_child_count, Equals(3));
            for (size_t i = 0; i < 3; i++)
                AssertThat(immediate_children[i], Equals(expected_children[i]));
        });
        
        it("removes any hidden nodes from its regular list of children", [&]() {
            ts_tree *expected_children[2] = {
                stack.entries[1].node,
                stack.entries[3].node,
            };
            
            ts_tree *node = ts_stack_reduce(&stack, sym2, 3, hidden_symbols, nullptr);
            size_t child_count;
            ts_tree **children = ts_tree_children(node, &child_count);
            
            AssertThat(child_count, Equals(2));
            for (size_t i = 0; i < 2; i++)
                AssertThat(children[i], Equals(expected_children[i]));
        });
        
        describe("when there are hidden nodes with children of their own", [&]() {
            ts_tree **grandchildren;
            ts_tree *hidden_node;
            
            before_each([&]() {
                grandchildren = tree_array({
                    ts_tree_make_leaf(sym1, 10, 2),
                    ts_tree_make_leaf(sym2, 10, 2),
                });
                
                hidden_node = ts_tree_make_node(hidden_sym, 2, 0, grandchildren);
                ts_stack_push(&stack, 21, hidden_node);
            });
            
            after_each([&]() {
                for (ts_state_id i = 0; i < 2; i++)
                    ts_tree_release(grandchildren[i]);
                free(grandchildren);
                ts_tree_release(hidden_node);
            });
            
            it("makes those child nodes children of the new node", [&]() {
                ts_tree *node = ts_stack_reduce(&stack, sym2, 4, hidden_symbols, nullptr);
                
                ts_tree *expected_children[4] = {
                    stack.entries[1].node,
                    stack.entries[3].node,
                    grandchildren[0],
                    grandchildren[1],
                };

                size_t child_count;
                ts_tree **children = ts_tree_children(node, &child_count);
                
                AssertThat(child_count, Equals(4));
                for (size_t i = 0; i < 4; i++)
                    AssertThat(children[i], Equals(expected_children[i]));
            });
        });
    });
});

END_TEST