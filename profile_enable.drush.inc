<?php
/**
 * @file
 * Provide Drush integration for (re)enabling Drupal installation profiles.
 */

/**
 * Implements hook_drush_help().
 */
function profile_enable_drush_help($section) {
  switch ($section) {
    case 'drush:profile-enable':
      return dt('(Re)Enable a Drupal installation profile. Support inherited profiles.');
  }
}

/**
 * Implements hook_drush_command().
 */
function profile_enable_drush_command() {
  $items = array();

  $items['profile-enable'] = array(
    'description' => 'Enable/Re-Enable a Drupal installation profile and all dependencies. Support inherited profiles.',
    'callback' => 'drush_profile_enable',
    'arguments' => array(
      'profile' => 'Name of the installation profile to (re)enable. Optional argument (defaults to current install_profile variable).',
    ),
    'examples' => array(
      'drush pe minimal' => '(Re)Enables the minimal profile and all it\'s dependencies',
    ),
    'aliases' => array('pe'),
  );

  return $items;
}

/**
 * Validate callback.
 *
 * Determine the dependencies of the profile that the user wants to enable.
 * This will incldue dependencies of base profiles and nested module
 * dependencies (if supported by the local Drupal installation).
 *
 * @param string $profile
 *   Name of a Drupal installation profile.
 */
function drush_profile_enable_validate($profile = NULL) {
  if (is_null($profile)) {
    $profile = variable_get('install_profile', 'standard');
  }

  drush_include_engine('drupal', 'environment');

  // Get the list of valid module names.
  $valid_module_names = array_keys(drush_get_modules());

  // Check if the requested profile name is valid.
  if (!in_array($profile, $valid_module_names)) {
    drush_log(dt('!extension was not found.', array('!extension' => $profile)), 'warning');
    return FALSE;
  }

  require_once DRUSH_DRUPAL_CORE . '/includes/install.inc';

  // Get the profile info.
  $profile_info = install_profile_info($profile);

  // Build out an array of dependencies for the install profile.
  $profile_dependencies = $profile_info['dependencies'];

  // Check back through any base profiles defined by the profile information.
  // This adds support for install profile inheritance that can be added to
  // Drupal via means of a core patch.
  //
  // @see https://www.drupal.org/node/1356276
  // @see https://www.drupal.org/node/2067229
  while (isset($profile_info['base profile']) && $profile_info['base profile']) {
    // Get the base profile info.
    $profile_info = install_profile_info($profile_info['base profile']);

    if (isset($profile_info['dependencies']) && is_array($profile_info['dependencies'])) {
      $profile_dependencies = array_unique(array_merge($profile_dependencies, $profile_info['dependencies']));
    }
  }

  // Fill the array keys with the values.
  $profile_dependencies = array_combine($profile_dependencies, $profile_dependencies);

  // Is possible recursively seek all dependencies; not just those
  // of the profiles.
  if (function_exists('_drupal_verify_profile_dependencies')) {
    $dependencies = array();

    foreach ($profile_dependencies as $profile_dependency) {
      _drupal_verify_profile_dependencies($dependencies, $profile_dependency);
    }

    // Append any module depdencies to the list of depdencies for this profile.
    $dependencies = array_keys($dependencies);
    $profile_dependencies = array_unique(array_merge($profile_dependencies, $dependencies));
  }

  // Get the list of present modules in the codebase.
  $present_modules = _profile_enable_get_present_modules();

  // Verify that all of the profile's required modules are present.
  $missing_modules = array();
  foreach ($profile_dependencies as $profile_dependency) {
    // Check early if the module even exists, continue early on.
    if (!in_array($profile_dependency, $present_modules)) {
      $missing_modules[] = $profile_dependency;
    }
  }

  // Show an error and bail out at this stage if there are any missing modules.
  if (count($missing_modules)) {
    drush_log(dt('The profile (!profile) could not be enabled as the following modules are not present: !missing', array(
      '!profile' => $profile,
      '!missing' => implode(', ', $missing_modules),
    )), 'warning');

    return FALSE;
  }

  // Get the list of enabled modules.
  $enabled_modules = drush_module_list();

  // Prepare the final list of dependencies that need to be enabled.
  $final_dependencies = array_diff($profile_dependencies, $enabled_modules);

  // Check the status of the requested profile.
  if (in_array($profile, $enabled_modules)) {
    // If it is already enabled, show a message.
    drush_log(dt('!profile is already enabled.', array('!profile' => $profile)), 'ok');
  }
  else {
    // Add the profile to the list of dependencies if not already enabled.
    $final_dependencies[$profile] = $profile;
  }

  // Remove any modules which do not meet the requirements to be enabled.
  foreach ($final_dependencies as $key => $module) {
    if (!drupal_check_module($module)) {
      unset($final_dependencies[$key]);
      drush_set_error('DRUSH_PM_ENABLE_MODULE_UNMEET_REQUIREMENTS', dt('Module !module doesn\'t meet the requirements to be enabled.', array('!module' => $module)));
      _drush_log_drupal_messages();
      return FALSE;
    }
  }

  // Set the final dependency list in the drush context to allow it to be
  // utilised in the drush callback function.
  drush_set_context('PROFILE_ENABLE_DEPENDENCIES', $final_dependencies);

  return TRUE;
}

/**
 * Command callback: Enable the profile selected by the user.
 *
 * This function will also enable all dependencies of the install profile as
 * determined by the validate function above.
 *
 * @param string $profile
 *   Name of a Drupal installation profile.
 *
 * @see drush_profile_enable_validate
 */
function drush_profile_enable($profile = NULL) {
  if (is_null($profile)) {
    $profile = variable_get('install_profile', 'standard');
  }

  // Get the list of dependencies that were built in the validate function.
  $profile_dependencies = drush_get_context('PROFILE_ENABLE_DEPENDENCIES');

  // If there are profile dependencies, ask the user for confirmation before we
  // just go an enable them..
  if (!empty($profile_dependencies) && is_array($profile_dependencies)) {
    drush_print(dt('The following extensions will be enabled: !extensions', array('!extensions' => implode(', ', $profile_dependencies))));
    if (!drush_confirm(dt('Do you really want to continue?'))) {
      return drush_user_abort();
    }
  }

  // Enable modules and pass dependency validation in form submit.
  if (!empty($profile_dependencies)) {
    drush_include_engine('drupal', 'environment');
    drush_module_enable($profile_dependencies);

    // Inform the user of the status of the profile dependencies.
    // Get the status of all dependencies we tried to enable.
    $result_extensions = drush_get_named_extensions_list($profile_dependencies);
  }

  // Keep track of all dependencies that did not enable correctly.
  if (isset($result_extensions) && count($result_extensions)) {
    $problem_dependencies = array();

    // Get the role handler class.
    $role = drush_role_get_class();

    // Loop through the dependencies.
    foreach ($result_extensions as $name => $extension) {
      // Check if the dependency is enabled.
      if ($extension->status) {
        // If the dependency enabled properly, let the user know.
        drush_log(dt('!extension was enabled successfully.', array('!extension' => $name)), 'ok');

        // Check if the dependency defind any permissions..
        $perms = $role->getModulePerms($name);

        // Let the user know about the permissions.
        if (!empty($perms)) {
          drush_print(dt('!extension defines the following permissions: !perms', array(
            '!extension' => $name,
            '!perms' => implode(', ', $perms),
          )));
        }
      }
      else {
        // This dependency couldn't be enabled.
        $problem_dependencies[] = $name;
      }
    }
  }

  // Alert the user of all dependencies that could not be enabled.
  if (!empty($problem_dependencies)) {
    return drush_set_error('DRUSH_PROFILE_ENABLE_DEPENDENCY_ISSUE', dt('There was a problem enabling !extension.', array('!extension' => implode(',', $problem_extensions))));
  }

  // Return the list of extensions enabled.
  return $profile_dependencies;
}

/**
 * Get a list of modules that exist in Drupal's assorted subdirectories.
 *
 * @return array
 *   List of modules present in the codebase.
 */
function _profile_enable_get_present_modules() {
  $present_modules = array();

  // Get a list of modules that exist in Drupal's assorted subdirectories.
  foreach (drupal_system_listing('/^' . DRUPAL_PHP_FUNCTION_PATTERN . '\.module$/', 'modules', 'name', 0) as $present_module) {
    $present_module_path = drupal_get_path('module', $present_module->name);
    $present_modules[] = $present_module->name;
  }

  return $present_modules;
}
