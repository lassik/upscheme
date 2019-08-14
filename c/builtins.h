value_t builtin_pid(value_t *args, uint32_t nargs);
value_t builtin_parent_pid(value_t *args, uint32_t nargs);
value_t builtin_process_group(value_t *args, uint32_t nargs);

value_t builtin_user_effective_gid(value_t *args, uint32_t nargs);
value_t builtin_user_effective_uid(value_t *args, uint32_t nargs);
value_t builtin_user_real_gid(value_t *args, uint32_t nargs);
value_t builtin_user_real_uid(value_t *args, uint32_t nargs);

value_t builtin_spawn(value_t *args, uint32_t nargs);

value_t builtin_read_ini_file(value_t *args, uint32_t nargs);

value_t builtin_color_name_to_rgb24(value_t *args, uint32_t nargs);
