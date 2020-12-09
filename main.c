#include <stdlib.h>
#include <stdio.h>
#include <string.h>
typedef enum
{
	CM_SUCCESS = 0,
	CM_FAIL = -1,
	CM_MALLOC_FAIL = -2,
	CM_ITEM_NOT_FOUND = -3,
	CM_MAP_ALREADY_EMPTY = -4,
	CM_BUFFER_TO_SMALL = -5,
	CM_IN_PROGRESS = -6,
	CM_NULL_KEY = -7,
	CM_NULL_MAP = -8,
	CM_NULL_CALLBACK = -9,
	CM_NULL_OUTPUT = -10
}result_t;

typedef void item_t;
typedef void key_t;

typedef void* (*allocator_func)(size_t);
typedef void  (*mem_free_func)(void*);
typedef int   (*mem_compare_func)(const key_t*, const key_t*, size_t);

typedef int   (*on_remove_func)(const key_t const*, item_t*);
typedef int   (*on_insert_func)(const key_t const*, item_t*);

typedef result_t   (*on_for_each_func)(size_t, const key_t const *, item_t*, void*);

typedef struct cmap_func_interface_t
{
	allocator_func alloc_func;
	mem_free_func free_func;
	mem_compare_func cmp_func;

	on_insert_func insert_callback;
	on_remove_func remove_callback;

}cmap_func_interface_t;

typedef struct
{
	key_t*  key;
	item_t* item;
}data_pack_t;

typedef struct node_t
{
	struct node_t* left;
	struct node_t* right;

	struct node_t* parent;

	data_pack_t data;
}node_t;

typedef struct cmap_t
{
	node_t* first_node;

	size_t item_type_size;
	size_t key_type_size;

	size_t items_count;

    cmap_func_interface_t interface;
}cmap_t;

void cm_interface_init_standard(cmap_func_interface_t* interface)
{
	interface->alloc_func = malloc;
	interface->free_func = free;
	interface->cmp_func = memcmp;

	interface->insert_callback = NULL;
	interface->remove_callback = NULL;
}

result_t cm_init_with_interface(cmap_t* cm, size_t item_size, size_t key_size, cmap_func_interface_t* interface)
{
	cm->interface = *interface;

	cm->key_type_size = key_size;
	cm->item_type_size = item_size;

	cm->first_node = NULL;
	cm->items_count = 0;

	return CM_SUCCESS;
}

result_t cm_init(cmap_t* cm, size_t item_size, size_t key_size)
{
	cmap_func_interface_t interface;
	cm_interface_init_standard(&interface);

	return cm_init_with_interface(cm,item_size,key_size,&interface);
}

int cm_is_empty(const cmap_t* cm)
{
	return cm->items_count == 0;
}

size_t cm_get_items_count(const cmap_t* cm)
{
	return cm->items_count;
}

size_t cm_get_max_items_count(const cmap_t* cm)
{
	return (size_t)pow(2, cm->items_count);
}

size_t cm_get_key_size(const cmap_t* cm)
{
	return cm->key_type_size;
}

size_t cm_get_item_size(const cmap_t* cm)
{
	return cm->item_type_size;
}

void node_destroy(const cmap_t* cm, node_t* destroyed_node)
{
	if (cm->interface.remove_callback != NULL)
	{
		cm->interface.remove_callback(destroyed_node->data.key, destroyed_node->data.item);
	}

	cm->interface.free_func(destroyed_node->data.key);
	cm->interface.free_func(destroyed_node->data.item);

	cm->interface.free_func(destroyed_node);
}

void connect_node(cmap_t* cm, node_t** existing_node, node_t** new_node)
{
	if (*existing_node == NULL)
	{
		(*new_node)->parent = *existing_node;
		*existing_node = *new_node;
	}
	else if (memcmp((*existing_node)->data.key, (*new_node)->data.key,cm->key_type_size) < 0)
	{
		connect_node(cm, &(*existing_node)->left, new_node);
	}
	else if (memcmp((*existing_node)->data.key, (*new_node)->data.key, cm->key_type_size) > 0)
	{
		connect_node(cm, &(*existing_node)->right, new_node);
	}
	else
	{
		/* Item with this key already exists - overwriting*/
		memcpy(&(*new_node)->data.item, &(*existing_node)->left, cm->item_type_size);
		node_destroy(cm,*new_node);
	}
}

node_t* node_create(cmap_t* cm, const key_t* key, const item_t* item)
{
	result_t ret_val;

	node_t* new_node = (node_t*)cm->interface.alloc_func(sizeof(node_t));
	if (new_node == NULL)
	{
		return NULL;
	};

	new_node->data.key = (key_t*)cm->interface.alloc_func(sizeof(cm->key_type_size));
	if (new_node->data.key != NULL)
	{
		memcpy(new_node->data.key, key, cm->key_type_size);
	}
	else
	{
		free(new_node);
		return NULL;
	}

	new_node->data.item = (item_t*)cm->interface.alloc_func(sizeof(cm->item_type_size));
	if (new_node->data.key != NULL)
	{
		memcpy(new_node->data.item, item, cm->item_type_size);
	}
	else
	{
		free(new_node->data.key);
		free(new_node);
		return NULL;
	}

	new_node->left = NULL;
	new_node->right = NULL;
	new_node->parent = NULL;

	return new_node;
}



result_t cm_push(cmap_t* cm, key_t * key, item_t * item)
{
	node_t* new_node = NULL;
	result_t ret_val = CM_SUCCESS;

	if (cm == NULL)
	{
		return CM_NULL_MAP;
	}

	if (key == NULL)
	{
		return CM_NULL_KEY;
	}

	new_node = node_create(cm, key, item);
	if (new_node == NULL)
	{
		ret_val = CM_MALLOC_FAIL;
	}

	connect_node(cm, &cm->first_node, &new_node);

	cm->items_count++;

	if (cm->interface.insert_callback != NULL)
	{
		cm->interface.insert_callback(new_node->data.key, new_node->data.item);
	}

	return ret_val;
}

node_t* node_get(cmap_t* cm, const key_t* key, node_t* checked_node)
{
	if (checked_node == NULL)
	{
		return NULL;
	}

	if (memcmp(checked_node->data.key, key, cm->key_type_size) < 0)
	{
		return node_get(cm, key, checked_node->left);
	}
	else if (memcmp(checked_node->data.key, key, cm->key_type_size) > 0)
	{
		return node_get(cm, key, checked_node->right);
	}

	return checked_node;
}

item_t* cm_get(cmap_t* cm, const key_t* key)
{
	node_t* found_node;

	if (key == NULL || cm == NULL)
	{
		return NULL;
	}

	found_node = node_get(cm, key, cm->first_node);

	return found_node != NULL ? found_node->data.item : NULL;
};

void node_disconnect(cmap_t* cm, node_t* parent, node_t* child)
{
	if (parent != NULL)
	{
		if (memcmp((parent)->data.key, (child)->data.key, cm->key_type_size) < 0)
		{
			parent->left = NULL;
		}
		if(memcmp((parent)->data.key, (child)->data.key, cm->key_type_size) > 0)
		{
			parent->right = NULL;
		}
	}
}
void node_reconnect_children(cmap_t* cm, node_t* parent, node_t* removed_node)
{
	if (removed_node->right != NULL)
	{
		connect_node(cm, &removed_node->parent, &removed_node->right);
	}
	if (removed_node->left != NULL)
	{
		connect_node(cm, &removed_node->parent, &removed_node->left);
	}
}

result_t cm_remove(cmap_t* cm, const key_t* key)
{
	node_t* removed_node = NULL;

	if (cm == NULL)
	{
		return CM_NULL_MAP;
	}

	if (key == NULL)
	{
		return CM_NULL_KEY;
	}

	removed_node = node_get(cm, key, cm->first_node);
	if (removed_node == NULL)
	{
		return CM_ITEM_NOT_FOUND;
	}

	node_disconnect(cm, removed_node->parent, removed_node);
	node_reconnect_children(cm, removed_node->parent, removed_node);
	node_destroy(cm, removed_node);

	cm->items_count--;

	return CM_SUCCESS;
};

void node_remove_recursive(cmap_t* cm, node_t* node)
{
	if (node != NULL)
	{
		node_remove_recursive(cm, node->left);
		node_remove_recursive(cm, node->right);

		node_destroy(cm, node);
	}
}

result_t cm_clear(cmap_t* cm)
{
	if (cm == NULL)
	{
		return CM_NULL_MAP;
	}

	if (cm_is_empty(cm))
	{
		return CM_MAP_ALREADY_EMPTY;
	}
	else
	{
		node_remove_recursive(cm,cm->first_node);
		cm->first_node = NULL;
	}
	return CM_SUCCESS;
}
result_t cm_destroy(cmap_t* cm)
{
	return cm_clear(cm);
}


void node_for_each_recursive(node_t* node, size_t * counter, on_for_each_func callback, void* arg, result_t * output_result)
{
	if (*output_result == CM_IN_PROGRESS && node != NULL)
	{
		*output_result = callback((*counter)++, node->data.key, node->data.item, arg);

		node_for_each_recursive(node->left,  counter, callback, arg, output_result);
		node_for_each_recursive(node->right, counter, callback, arg, output_result);
	}
}

result_t cm_for_each(cmap_t* cm, on_for_each_func callback, void* arg)
{
	size_t item_counter = 0;
	result_t result = CM_IN_PROGRESS;

	if (cm == NULL)
	{
		return CM_NULL_MAP;
	}

	if (cm == NULL)
	{
		return CM_NULL_CALLBACK;
	}

	node_for_each_recursive(cm->first_node, &item_counter, callback, arg, &result);

	return result == CM_SUCCESS || result == CM_IN_PROGRESS ? CM_SUCCESS : result;
}

result_t key_to_arr(size_t index, const key_t const* key, item_t* item, void* arg)
{
	const key_t const ** key_arr = (const key_t const**)arg;

	key_arr[index] = key;

	return CM_IN_PROGRESS;
}

result_t cm_keys_to_array(cmap_t* cm, const key_t const* output_array[], size_t array_size)
{
	if (cm == NULL)
	{
		return CM_NULL_MAP;
	}

	if (output_array == NULL)
	{
		return CM_NULL_OUTPUT;
	}

	if (cm_get_items_count(cm) > array_size)
	{
		return CM_BUFFER_TO_SMALL;
	}

	return cm_for_each(cm, key_to_arr, output_array);
}

result_t item_to_arr(size_t index, const key_t const* key, item_t* item, void* arg)
{
	item_t** iten_arr = (item_t **)arg;

	iten_arr[index] = item;

	return CM_IN_PROGRESS;
}

result_t cm_items_to_array(cmap_t* cm, item_t * output_array[], size_t array_size)
{
	if (cm == NULL)
	{
		return CM_NULL_MAP;
	}

	if (output_array == NULL)
	{
		return CM_NULL_OUTPUT;
	}

	if (cm_get_items_count(cm) > array_size)
	{
		return CM_BUFFER_TO_SMALL;
	}

	return cm_for_each(cm, item_to_arr, output_array);
}


typedef struct
{
	const key_t const* output;
	item_t* checked_item;
	size_t item_size;

}item_finder_t;

result_t find_item(size_t index, const key_t const* key, item_t* item, void* arg)
{
	item_finder_t* it_finder = (item_finder_t*)arg;

	if (memcmp(item, it_finder->checked_item, it_finder->item_size) == 0)
	{
		it_finder->output = key;
		return CM_SUCCESS;
	}

}
const key_t const* cm_get_key(cmap_t* cm, item_t* item)
{
	item_finder_t item_finder = { NULL, NULL, 0 };

	if (cm == NULL)
	{
		return NULL;
	}

	item_finder.item_size = cm->item_type_size;

	cm_for_each(cm, find_item, &item_finder);
	return  item_finder.output;
}
result_t print_item(size_t index, const key_t const* key, item_t* item, void* arg)
{
	printf("%llu. key: %d, item: %d\n",index,*(int*)key, *(int*)item);
	return CM_IN_PROGRESS;
}


int main()
{
	cmap_t m;
	cm_init(&m, sizeof(int), sizeof(int));

	for(int i = 0; i < 7; i++)
	{
		int a = i * i;
		cm_push(&m, &i, &a);
	}

	cm_for_each(&m,print_item,NULL);

	cm_destroy(&m);
}
