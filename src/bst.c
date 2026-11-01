/**
 * @file bst.c
 * @brief File for the AVL tree implementation
 *
 * Project: Implementace překladače imperativního jazyka IFJ2026
 *
 * @author Arkhyp Samoilenko (xsamoia00)
 *
 */

#include "bst.h"

#include <stdlib.h>
#include <string.h>

/**
 * @brief Structure for AVL tree's node
 */
struct bstNode
{
    char* key;             /**< key for comparing and placing */
    nodeData* data;        /**< abstact data type implemented in symtable.c */
    int height;            /**< height of a node */
    struct bstNode* left;  /**< ptr to left child */
    struct bstNode* right; /**< ptr to right child */
};

static bstNode* allocateNode(const char* key, nodeData* data);
static int getHeight(const bstNode* node);
static int getMax(int a, int b);
static bstStatus placeNode(const char* newKey, bstNode** parent,
                           nodeData* data);
static void rotateLeft(bstNode** parent);
static void rotateRight(bstNode** parent);


void bstInit(bstNode** root)
{
    if (root != NULL)
    {
        *root = NULL;
    }
}
bstStatus bstInsert(bstNode** root, const char* key, nodeData* data)
{
    if (root == NULL || key == NULL)
    {
        return BST_ERROR;
    }
    return placeNode(key, root, data);
}

static bstStatus placeNode(const char* newKey, bstNode** parent, nodeData* data)
{
    /** check for a first insertion
     * and last insertion to either left of right child of a previously
     * childless parent
     */
    if (*parent == NULL)
    {
        *parent = allocateNode(newKey, data);
        if (*parent == NULL)
        {
            return BST_ERROR;
        }
        return BST_INSERTED;
    }
    /** comparing both keys using strcmp */
    int cmpResult = strcmp(newKey, (*parent)->key);
    /** variable to save the return value of the recursive function.
     *  if no node was added, ignore everything and delive error code to the
     *  caller (bstInsert)
     */
    bstStatus wasPlaced = BST_INSERTED;
    /** case 1: newKey is less (in terms of ASCII table) than parent's key */
    if (cmpResult < 0)
    {
        wasPlaced = placeNode(newKey, &((*parent)->left), data);
    }

    /** case 2: newKey is larger (in terms of ASCII table) than parent's key */
    else if (cmpResult > 0)
    {
        wasPlaced = placeNode(newKey, &((*parent)->right), data);
    }

    /** case 3: both keys are equal */
    else
    {
        return BST_EXISTS;
    }
    if (wasPlaced != BST_INSERTED)
    {
        return wasPlaced;
    }
    /** height of a node recalibration in case of a changed height */
    int heightL = getHeight((*parent)->left);
    int heightR = getHeight((*parent)->right);
    (*parent)->height = 1 + getMax(heightL, heightR);
    /** get the balance of the current node */
    int balance = heightL - heightR;

    /** case 1: unbalanced toward left side */
    if (balance > 1)
    {
        /** left-right turn in case of a zig-zag node structure */
        if (strcmp((*parent)->left->key, newKey) < 0)
        {
            rotateLeft(&(*parent)->left);
            rotateRight(parent);
            return BST_INSERTED;
        }
        /** basic right turn */
        rotateRight(parent);
        return BST_INSERTED;
    }
    /** case 2: unbalanced toward right side */
    else if (balance < -1)
    {
        /** right-left turn in case of a zig-zag node structure */
        if (strcmp((*parent)->right->key, newKey) > 0)
        {
            rotateRight(&(*parent)->right);
            rotateLeft(parent);
            return BST_INSERTED;
        }
        /** basic left turn */
        rotateLeft(parent);
        return BST_INSERTED;
    }
    return BST_INSERTED;
}

static int getHeight(const bstNode* node)
{
    if (node == NULL)
    {
        return 0;
    }
    return node->height;
}


static int getMax(int a, int b)
{
    return a < b ? b : a;
}


static bstNode* allocateNode(const char* key, nodeData* data)
{
    bstNode* newNode = malloc(sizeof(bstNode));
    if (newNode == NULL)
    {
        return NULL;
    }
    newNode->key = malloc(strlen(key) + 1);
    if (newNode->key == NULL)
    {
        free(newNode);
        return NULL;
    }
    strcpy(newNode->key, key);

    /** setting basic parameters in newNode to basic values */
    newNode->height = 1;
    newNode->left = NULL;
    newNode->right = NULL;
    newNode->data = data;
    return newNode;
}
static void rotateLeft(bstNode** parent)
{
    /** copy into temp the ptr of right child of former parent */
    bstNode* temp = (*parent)->right;
    /** give former parent the branch that is smaller than the new parent */
    (*parent)->right = (*parent)->right->left;
    /** put former parent as a left child of his former right child */
    temp->left = *parent;
    /** insert ptr of a new parent in the place of ptr of the former parent */
    *parent = temp;

    /** change height of new parent and its left child (former parent) */
    (*parent)->left->height = 1 + getMax(getHeight((*parent)->left->left),
                                         getHeight((*parent)->left->right));
    (*parent)->height =
        1 + getMax(getHeight((*parent)->left), getHeight((*parent)->right));
}
static void rotateRight(bstNode** parent)
{
    /** copy into temp the ptr of left child of former parent */
    bstNode* temp = (*parent)->left;
    /** give former parent the branch that is bigger than the new parent */
    (*parent)->left = (*parent)->left->right;
    /** put former parent as a right child of his former left child */
    temp->right = *parent;
    /** insert ptr of a new parent in the place of ptr of the former parent */
    *parent = temp;

    /** change height of new parent and its right child (former parent) */
    (*parent)->right->height = 1 + getMax(getHeight((*parent)->right->left),
                                          getHeight((*parent)->right->right));
    (*parent)->height =
        1 + getMax(getHeight((*parent)->left), getHeight((*parent)->right));
}

nodeData* bstSearch(bstNode* root, const char* key)
{
    /** check whether current root is the end of a branch */
    if (root == NULL)
    {
        return NULL;
    }
    /** comparing keys of searched node and deciding where it should go */
    int cmpResult = strcmp(key, root->key);
    /** case 1: it will go to the left branch */
    if (cmpResult < 0)
    {
        return bstSearch(root->left, key);
    }
    /** case 2: it will go to the right branch */
    else if (cmpResult > 0)
    {
        return bstSearch(root->right, key);
    }
    /** matching keys means return data from searched node */
    else
    {
        return root->data;
    }
}

void bstDestroy(bstNode** root, dataEraser eraseDataFunc)
{
    /** first check whether given address of ptr root is valid
     * then if given address is ok it can proceed to check whether it's in
     * existing node and start backtracing to last real node
     */
    if (root == NULL || *root == NULL)
    {
        return;
    }
    /** post order traversal before freeing current node */
    bstDestroy(&(*root)->left, eraseDataFunc);
    bstDestroy(&(*root)->right, eraseDataFunc);

    free((*root)->key);


    eraseDataFunc((*root)->data);

    /** freeing root and assigning value of root ptr to point towards null*/
    free(*root);
    *root = NULL;
}