<?php
/**
 * @file
 * Manages application of conversion routines, logging, and patch file creation.
 *
 * The functions in these conversion routine files correspond to the topics in
 * the category roadmap at http://drupal.org/node/394070 that are marked with
 * a green check mark in the Upgrade column.
 *
 * Copyright 2008-11 by Jim Berry ("solotandem", http://drupal.org/user/240748)
 */

/**
 * Prepares conversion environment and starts conversion loop.
 *
 * @param array $upgrades
 *   Array of upgrade sets to apply.
 * @param array $extensions
 *   Array of file types to convert based on extension.
 * @param array $items
 *   Array of directories containing the files to convert.
 * @param boolean $recursive
 *   Indicates whether to recurse the subdirectories of each $item.
 *
 * @return boolean
 *   Indicates whether the conversion code was successfully applied.
 */
function coder_subthemer_start($extensions, $items, $recursive = TRUE) {
  // Declare global variables.
  global $_coder_upgrade_log, $_coder_upgrade_debug, $_coder_upgrade_module_subtheme_name, $_coder_upgrade_module_name, $_coder_upgrade_replace_files, $_coder_upgrade_class_files;
//  global $_coder_upgrade_dirname; // Not used.

  if (!is_array($extensions) || empty($extensions)) {
    return FALSE;
  }
  if (!is_array($items) || empty($items)) {
    return FALSE;
  }

  $config = config('coder_upgrade.settings');
  $_coder_upgrade_log = TRUE;
  if ($_coder_upgrade_log) {
    // Clear the log file.
    coder_upgrade_path_clear('log');
    if (!$config->get('coder_upgrade_use_separate_process')) {
      coder_upgrade_path_clear('memory');
    }
    coder_upgrade_memory_print('initial');
  }
  // Set debug output preference.
  $_coder_upgrade_debug = $config->get('coder_upgrade_enable_debug_output');
  if ($_coder_upgrade_debug) {
    // Clear the debug file.
    coder_upgrade_path_clear('debug');
  }

  // Load code.
  $upgrades = array();
  //coder_upgrade_load_code(array('coder_upgrade' => $upgrades['coder_upgrade']));
  coder_upgrade_load_parser();

  // Set file replacement parameter.
  $_coder_upgrade_replace_files = $config->get('coder_upgrade_replace_files');
  // Initialize list of class files.
  // $_coder_upgrade_class_files = array();

  // Loop on items.
  foreach ($items as $item) {
    // Set the module name so the <convert_begin_alter> functions have this.
    $_coder_upgrade_module_name = $item['name'];
    $_coder_upgrade_module_subtheme_name = $item['values']['subtheme_name'];

    if (!isset($_SERVER['HTTP_USER_AGENT']) || strpos($_SERVER['HTTP_USER_AGENT'], 'simpletest') === FALSE) {
      // Process the directory before conversion routines are applied.
      // Note: if user agent is not set, then this is being called from CLI.
      coder_subthemer_convert_begin($item);
    }

    // Call main conversion loop.
    coder_subthemer_convert_dir($upgrades, $extensions, $item, $recursive);

    // Apply finishing touches to the directory.
    // Swap directories if files are replaced.
    $new_dir = $_coder_upgrade_replace_files ? $item['old_dir'] : $item['new_dir'];
    coder_subthemer_convert_end($new_dir);

    // Make a patch file.
    coder_upgrade_make_patch_file($item, $_coder_upgrade_replace_files);
  }

  return TRUE;
}

/**
 * Processes the directory before conversion routines are applied.
 *
 * This hook can be used to cache information needed by other routines.
 * Example: core changes need to know about hook_theme or hook_menu to make
 * theme changes and form API changes.
 *
 * @param array $item
 *   Array of a directory containing the files to convert.
 */
function coder_subthemer_convert_begin($item) {
  $dirname = $item['old_dir'];
  coder_upgrade_log_print("\n*************************");
  coder_upgrade_log_print('Pre-processing the directory => ' . $dirname);
  coder_upgrade_log_print("*************************");
  coder_upgrade_log_print("Calling hook_upgrade_begin_alter");
  backdrop_alter('subthemer_upgrade_begin', $item);
  coder_upgrade_log_print("Completed hook_upgrade_begin_alter");
}

/**
 * Applies finishing touches to the directory of converted files.
 *
 * @param string $dirname
 *   The name of the directory with the converted files.
 */
function coder_subthemer_convert_end($dirname) {
//  $dirname = $item['old_dir'];
  coder_upgrade_log_print("\n*************************");
  coder_upgrade_log_print('Post-processing the directory => ' . $dirname);
  coder_upgrade_log_print("*************************");
  coder_upgrade_log_print("Calling hook_upgrade_end_alter");
  backdrop_alter('subthemer_upgrade_end', $dirname);
  coder_upgrade_log_print("Completed hook_upgrade_end_alter");
}

/**
 * Converts files in a directory.
 */
function coder_subthemer_make_dir_list($items) {
  $values = $items[0]['values'];
}

/**
 * Converts files in a directory.
 *
 * @param array $upgrades
 *   Array of upgrade sets to apply.
 * @param array $extensions
 *   Array of file types to convert based on extension.
 * @param array $item
 *   Array of a directory containing the files to convert.
 * @param boolean $recursive
 *   Indicates whether to recurse the subdirectories of $item.
 */
function coder_subthemer_convert_dir($upgrades, $extensions, $item, $recursive = TRUE) {
  global $_coder_upgrade_module_name, $_coder_upgrade_replace_files;

  $dirname = $item['old_dir'];
  $new_dirname = $item['new_dir'];

  // Create an output directory we can write to.
  if (!is_dir($new_dirname)) {
    mkdir($new_dirname);
    chmod($new_dirname, 0757);
  }
  else {
    coder_upgrade_clean_directory($new_dirname);
  }

  // Determine module name.
  coder_upgrade_module_name($dirname, $item);
  $_coder_upgrade_module_name = !empty($item['module']) ? $item['module'] : $_coder_upgrade_module_name;

  // Create an info file, copy the favicon, logo, copy template.php
  $allowed_filenames = array(
    'favicon.ico',
    'logo.png',
  );
  if (!empty($item['values']['copy_settings'])) {
    $allowed_filenames[] = 'theme-settings.php';
  }

  backdrop_alter('coder_subthemer_allowed_filenames', $allowed_filenames);
    
  foreach ($allowed_filenames as $filename) {
    copy($dirname . '/' . $filename, $new_dirname . '/' . $filename);
    coder_subthemer_convert_file($dirname . '/' . $filename, $new_dirname . '/' . $filename, $_coder_upgrade_replace_files = FALSE);
  }

  // Create a color directory if needed.
  if (!empty($item['values']['copy_color'])) {
    $copy_color_item = array(
      'name' => $item['name'],
      'old_dir' => $dirname . '/color',
      'new_dir' => $new_dirname . '/color',
    );
    coder_subthemer_copy_dir($upgrades, $extensions, $copy_color_item, $recursive);
  }

  // Create a templates directory if needed.
  // Copy over the tpl.php files.
  if (isset($item['values']['template_list'])) {
    $template_list = array_filter($item['values']['template_list']);
    if (!empty($template_list)) {
      $template_directory = $new_dirname . '/templates';
      // Todo: this was mkdir. May need to change if this is modified for CLI.
      file_prepare_directory($template_directory, FILE_CREATE_DIRECTORY);
      foreach ($template_list as $template_path) {
        $file = basename($template_path);         // $file is set to "index.php"
        $dest = $template_directory . '/' . $file;
        $source = BACKDROP_ROOT . '/' . $template_path;
        copy($source, $dest);
      }
    }
  }

  // Create CSS an JS folders.
  if (!empty($item['values']['styles'])) {
    foreach ($item['values']['styles'] as $style) {
      $pathinfo = pathinfo($style);
      $path = $new_dirname . '/' . $pathinfo['dirname'];
      file_prepare_directory($path, FILE_CREATE_DIRECTORY);
      file_put_contents($new_dirname . '/' . $style, '');
    }
  }

  if (!empty($item['values']['scripts'])) {
    foreach ($item['values']['scripts'] as $script) {
      $pathinfo = pathinfo($script);
      $path = $new_dirname . '/' . $pathinfo['dirname'];
      file_prepare_directory($path, FILE_CREATE_DIRECTORY);
      file_put_contents($new_dirname . '/' . $script, '');
    }
  }
  
  // Modify the template.php code.
  if (!empty($item['values']['copy_template_php'])) {
    $template_php_content = "<?php\n";
    $template_php_content .= "/**\n";
    $template_php_content .= " * @file\n";
    $template_php_content .= " * " . $item['values']['name'] . " preprocess functions and theme function overrides.\n";
    $template_php_content .= " */\n";

    file_put_contents($new_dirname . '/template.php', $template_php_content);
  }

  // Build the info file.
  $dirs = system_rebuild_theme_data();
  $data = $dirs[$item['name']]->info;
  $new_data['name'] = $item['values']['name'];
  $new_data['description'] = t('Subtheme of ') . $item['name'];
  $new_data['type'] = 'theme';
  $new_data['backdrop'] = '1.x';
  $new_data['base theme'] = $item['name'];
  $new_data['newline1'] = 'newline';

  if (!empty($item['values']['styles'])) {
    $new_data['stylesheets'] = array('all' => backdrop_map_assoc($item['values']['styles']));
  }
  $new_data['newline2'] = 'newline';

  if (!empty($item['values']['styles'])) {
    $new_data['scripts'] = $item['values']['scripts'];
  }
  $new_data['newline3'] = 'newline';

  if (empty($item['values']['copy_settings']) && !empty($item['values']['copy_color'])) {
    $new_data['settings']['color'] = 'true';
  }

  backdrop_alter('coder_subthemer_build_info_file', $new_data);

  $info_file = coder_subthemer_build_info_file($new_data);
  $info_file_path = $new_dirname . '/' . $item['values']['subtheme_name'] . '.info';
  file_put_contents($info_file_path, $info_file);

}

/**
 * Converts files in a directory.
 *
 * @param array $upgrades
 *   Array of upgrade sets to apply.
 * @param array $extensions
 *   Array of file types to convert based on extension.
 * @param array $item
 *   Array of a directory containing the files to convert.
 * @param boolean $recursive
 *   Indicates whether to recurse the subdirectories of $item.
 */
function coder_subthemer_copy_dir($upgrades, $extensions, $item, $recursive = TRUE) {
  global $_coder_upgrade_filename; // Not used by this module, but other modules may find it useful.
  global $_coder_upgrade_module_name, $_coder_upgrade_replace_files;

  $dirname = $item['old_dir'];
  $new_dirname = $item['new_dir'];

  // Create an output directory we can write to.
  if (!is_dir($new_dirname)) {
    mkdir($new_dirname);
    chmod($new_dirname, 0757);
  }
  else {
    coder_upgrade_clean_directory($new_dirname);
  }

  static $ignore = array(/*'.', '..', '.bzr', '.git', '.svn',*/ 'CVS');
  if (!in_array($dirname, $ignore)) {
    coder_upgrade_log_print("\n*************************");
    coder_upgrade_log_print('Converting the directory => ' . $dirname);
    coder_upgrade_log_print("*************************");
  }

  // Loop on files.
  $filenames = scandir($dirname . '/');
  foreach ($filenames as $filename) {
      $_coder_upgrade_filename = $dirname . '/' . $filename;
      if (is_dir($dirname . '/' . $filename)) {
        // Todo: some files may be in .file format but necessary.
        if (substr(basename($filename), 0, 1) == '.' || in_array(basename($filename), $ignore)) {
          // Ignore all hidden directories and CVS directory.
          continue;
        }
        $new_filename = $filename;
        if ($recursive) {
          // TODO Fix this!!!
          $new_item = array(
            'name' => $item['name'],
            'old_dir' => $dirname . '/' . $filename,
            'new_dir' => $new_dirname . '/' . $filename,
          );
          coder_subthemer_copy_dir($upgrades, $extensions, $new_item, $recursive);
        }
      }
      elseif (!is_dir($dirname . '/' . $filename) && in_array($extension = pathinfo($filename, PATHINFO_EXTENSION), array_keys($extensions))) {
        $new_filename = $filename;
        if (strpos($filename, $_coder_upgrade_module_name) === 0) {
          $new_filename = str_replace($_coder_upgrade_module_name, $item['values']['subtheme_name'], $filename);
        }
        copy($dirname . '/' . $filename, $new_dirname . '/' . $new_filename);
        coder_upgrade_log_print("\n*************************");
        coder_upgrade_log_print('Converting the file => ' . $filename);
        coder_upgrade_log_print("*************************");
        coder_subthemer_convert_file($dirname . '/' . $filename, $new_dirname . '/' . $new_filename, $_coder_upgrade_replace_files);
      }
      elseif (!is_dir($dirname . '/' . $filename)) {
        $new_filename = $filename;
        if (strpos($filename, $_coder_upgrade_module_name) === 0) {
          $new_filename = str_replace($_coder_upgrade_module_name, $item['values']['subtheme_name'], $filename);
        }
        copy($dirname . '/' . $filename, $new_dirname . '/' . $new_filename);
      }
  }
}

/**
 * Converts a file.
 *
 * @param string $oldname
 *   The original name of the file to convert.
 * @param string $filename
 *   The new name of the file to convert.
 * @param boolean $_coder_upgrade_replace_files
 *   Indicates whether to replace the original files.
 */
function coder_subthemer_convert_file($oldname, $filename, $_coder_upgrade_replace_files = FALSE) {
  if (!file_exists($filename)) {
    return FALSE;
  }

  $extension = pathinfo($filename, PATHINFO_EXTENSION);

  // Read the file and copy the contents.
  $cur = file_get_contents($filename);
  $new = $cur;

  // Apply parser routines.
  if ($extension != 'js') {
    coder_subthemer_apply_parser($filename, $new);
  }
  // Apply regular expression routines.
  coder_subthemer_apply_regex($filename, $new);

  // Write the new file.
  if ($new != $cur) {
    $filename = $_coder_upgrade_replace_files ? $oldname : $filename;
    if (file_put_contents($filename, $new) === FALSE) {
      coder_upgrade_log_print('File could not be written');
    }
    coder_upgrade_log_print('Replaced the file');
  }
}

/**
 * Applies regular expression conversion routines to a file.
 *
 * @param string $filename
 *   The name of the file to convert.
 * @param string $new
 *   The contents of the file to convert.
 */
function coder_subthemer_apply_regex($filename, &$new) {
  // Categorize certain files.
  $extension = pathinfo($filename, PATHINFO_EXTENSION);
  $is_js_file = $extension == 'js';
  $is_info_file = $extension == 'info';

  if ($is_js_file) {
    // Apply regular expression conversion routines for js file.
    coder_upgrade_log_print("Calling hook_upgrade_regex_js_alter");
    backdrop_alter('subthemer_upgrade_regex_js', $new);
    coder_upgrade_log_print("Completed hook_upgrade_regex_js_alter");
    return;
  }
  // Apply regular expression conversion routines.
  coder_upgrade_log_print("Calling hook_upgrade_regex_alter");
  backdrop_alter('subthemer_upgrade_regex', $new);
  coder_upgrade_log_print("Completed hook_upgrade_regex_alter");
}

/**
 * Applies grammar parser conversion routines to a file.
 *
 * @param string $filename
 *   The name of the file to convert.
 * @param string $new
 *   The contents of the file to convert.
 */
function coder_subthemer_apply_parser($filename, &$new) {
  $config = config('coder_upgrade.settings');
  global $_coder_upgrade_class_files;
  // Categorize certain files.
  $is_info_file = pathinfo($filename, PATHINFO_EXTENSION) == 'info';
  $is_install_file = pathinfo($filename, PATHINFO_EXTENSION) == 'install';
  if ($is_info_file) {
    // Grammar parser only parses PHP code files.
    return;
  }

  // Create reader object.
  $reader = PGPReader::getInstance();
  coder_upgrade_memory_print('create reader for file ' . $filename);
  // Set array formatting preference.
  $reader->setPreserveArrayFormat($config->get('coder_upgrade_preserve_array_format'));
  // Set debug output preference.
  $reader->setDebug($config->get('coder_upgrade_enable_parser_debug_output'));
  $reader->setSnippet($new);
  coder_upgrade_memory_print('set snippet');
  $reader->addTokenNames();
  coder_upgrade_memory_print('add token names');
  // Parse code file.
  $reader->buildGrammar();
  coder_upgrade_memory_print('build grammar');

  $nodes = &$reader->getClasses();
  if (!empty($nodes)) {
    $_coder_upgrade_class_files[] = $filename;
  }

  // Apply parser conversion routines for function calls.
  coder_upgrade_log_print("Calling hook_upgrade_call_alter");
  coder_subthemer_convert_function_calls($reader);
  coder_upgrade_log_print("Completed hook_upgrade_call_alter");
  coder_upgrade_memory_print('apply function call conversions');

  // Apply parser conversion routines for hook functions.
  coder_upgrade_log_print("Calling hook_upgrade_hook_alter");
  coder_subthemer_convert_functions($reader);
  coder_upgrade_log_print("Completed hook_upgrade_hook_alter");
  coder_upgrade_memory_print('apply hook function conversions');

  // Apply parser conversion routines for the file.
  coder_upgrade_log_print("Calling hook_upgrade_file_alter");
  backdrop_alter('subthemer_upgrade_file', $reader);
  coder_upgrade_log_print("Completed hook_upgrade_file_alter");
  coder_upgrade_memory_print('apply file conversions');

  // Use writer to redo file.
  $writer = PGPWriter::getInstance();
  coder_upgrade_memory_print('create writer');
  $new = $writer->toString($reader->getStatements());
  coder_upgrade_memory_print('writer->toString');

  // Free up memory.
  $reader->reset();
  coder_upgrade_memory_print('reset reader');
}

/**
 * Upgrades function calls using grammar parser.
 *
 * @param PGPReader $reader
 *   The object containing the grammar statements of the file to convert.
 */
function coder_subthemer_convert_function_calls(&$reader) {
  cdp("inside " . __FUNCTION__);
  $nodes = &$reader->getFunctionCalls();
  foreach ($nodes as &$node) {
    $item = &$node->data;
    if (!isset($item) || !is_object($item) || !($item instanceof PGPFunctionCall) || $item->type != T_FUNCTION_CALL) {
      // The reference could have been changed in another routine so that it
      // no longer refers to an object.
      continue;
    }
    // If need to change other statements that we build as a function call,
    // then modify the next line. Others: eval, empty, unset, print, throw.
    $types = array(T_STRING, T_REQUIRE, T_REQUIRE_ONCE, T_INCLUDE, T_INCLUDE_ONCE);
    if (is_array($item->name) && in_array($item->name['type'], $types)) {
      // If name is an object, then it is a variable expression that is not
      // 'alterable' in the traditional sense.
      backdrop_alter('subthemer_upgrade_call_' . $item->name['value'], $node, $reader);
      // TODO The alter hooks can get the $reader from PGPReader::getInstance();
      // We could do the same in this function.
    }

    if (!isset($node) || !is_object($node) || !($node instanceof PGPNode)) {
      continue;
    }
    $item = &$node->data;
    if (!isset($item) || !is_object($item) || !($item instanceof PGPFunctionCall) || $item->type != T_FUNCTION_CALL) {
      continue;
    }
    if (is_array($item->name) && in_array($item->name['type'], $types)) {
      // Set name because only variables may be passed by reference.
      $name = $item->name['value'];
      backdrop_alter('subthemer_upgrade_call', $node, $reader, $name);
      // TODO The alter hooks can get the $reader from PGPReader::getInstance();
      // We could do the same in this function.
    }
  }
}

/**
 * Upgrades functions (or hooks) using grammar parser.
 *
 * @param PGPReader $reader
 *   The object containing the grammar statements of the file to convert.
 */
function coder_subthemer_convert_functions(&$reader) {
  cdp("inside " . __FUNCTION__);
  global $_coder_upgrade_module_name;

  $nodes = &$reader->getFunctions();
  foreach ($nodes as &$node) {
    $item = &$node->data;
    if (!isset($item) || !is_object($item) || !($item instanceof PGPClass) || $item->type != T_FUNCTION) {
      // The reference could have been changed in another routine so that it
      // no longer refers to an object.
      continue;
    }

    $name = &$item->name;
    cdp("name = $name");


    /*
     * If the function name does not begin with the module name, then ignore it.
     * This assumes such a function would be an instance of an API hook defined
     * by the contributed module but implemented on behalf of another module. For
     * this use case, the contributed module would define upgrade routines to
     * allow other contributed modules that implement said API to upgrade their
     * code.
     *
     * Example: the Views module defines hooks and implements them on behalf of
     * core modules.
     *
     * Strip the module name from the function name and use this as the key in
     * a switch statement. In some cases (e.g. hook_update_N), some additional
     * manipulation and checking needs to be done.
     */
    if (strpos($name, $_coder_upgrade_module_name . '_') !== 0 && strpos($name, 'theme_') !== 0) {
      clp("Ignoring function '$name' as its name does not begin with the module name or 'theme_'");
      continue;
    }

    // By convention, the module name should be followed by an underscore.
    $hook = substr($name, strlen($_coder_upgrade_module_name) + 1);
    cdp("hook = $hook");

    // Update hooks need additional manipulation.
    if (preg_match('@update_\d+$@', $hook, $matches)) {
      $hook = 'update_N';
    }

    // TODO The alter hooks can get the $reader from PGPReader::getInstance();
    // We could do the same in this function.
    backdrop_alter('subthemer_upgrade_hook_' . $hook, $node, $reader);

    if (!isset($item) || !is_object($item) || !($item instanceof PGPClass) || $item->type != T_FUNCTION) {
      continue;
    }
    if (strpos($name, $_coder_upgrade_module_name . '_') !== 0 && strpos($name, 'theme_') !== 0) {
      clp("Ignoring function '$name' as its name does not begin with the module name or 'theme_'");
      continue;
    }
    backdrop_alter('subthemer_upgrade_hook', $node, $reader, $hook);
  }
}

/**
 * Implements hook_subthemer_upgrade_regex_alter().
 *
 * Alters the text of a code file using regular expressions.
 *
 * @param string $file
 *   The text of the file to convert.
 */
function coder_subthemer_subthemer_upgrade_regex_alter(&$file) {
  cdp("inside " . __FUNCTION__);
  global $_coder_upgrade_module_name;
  global $_coder_upgrade_module_subtheme_name;

  $hook = 'info_file';
  $cur = $file;
  $new = $cur;

  $from = array();
  $to = array();

  // Change the base theme.
  $from[] = $_coder_upgrade_module_name;
  $to[] = $_coder_upgrade_module_subtheme_name;

  coder_upgrade_do_conversions($from, $to, $new);
  coder_upgrade_save_changes($cur, $new, $file, $hook);
}

/**
 * Adds the module name to the item array.
 *
 * @param string $dirname
 *   A string of the directory name.
 * @param array $item
 *   Array of a directory containing the files to convert.
 */
function coder_subthemer_module_name($dirname, &$item) {
  // Extensions that indicate a module is present.
  $extensions = array('info', 'module');

  /*
   * Set the module name in case there is no module in the directory (e.g. po
   * or translations).
   *
   * This code assumes at most one module per directory. Absent this condition
   * we have no way of determining the file list for the .info file. This
   * condition does not hold for the devel project which has 4 modules in its
   * top-level directory.
   */
  $item['module'] = '';

  // Loop on files.
  $path = $dirname . '/';
  $files = scandir($path);
  foreach ($files as $file) {
    $file_path = $path . $file;
    if (!is_dir($file_path)) {
      if (in_array(pathinfo($file_path, PATHINFO_EXTENSION), $extensions)) {
        $item['module'] = pathinfo($file_path, PATHINFO_FILENAME);
        break;
      }
    }
  }
}

/**
 * Builds an array of candidates for modules and submodules.
 *
 * @param string $dirname
 *   A string of the directory name.
 */
function coder_subthemer_module_files($dirname) {
  // Extensions that indicate a module is present.
  $extensions = array('module');

  static $item = array();

  $path = $dirname . '/';
  $files = scandir($path);
  foreach ($files as $file) {
    if ($file == '.' || $file == '..') {
      continue;
    }
    $file_path = $path . $file;
    if (!is_dir($file_path)) {
      if (in_array(pathinfo($file_path, PATHINFO_EXTENSION), $extensions)) {
        $name = pathinfo($file_path, PATHINFO_FILENAME);
        $item[$name] = $path;
      }
    }
    else {
      coder_subthemer_module_files($file_path);
    }
  }
  return $item;
}
