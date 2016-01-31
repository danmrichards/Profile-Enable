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
      'profile' => 'Name of the installation profile to (re)enable.',
    ),
    'examples' => array(
      'drush pe minimal' => '(Re)Enables the minimal profile and all it\'s dependencies',
    ),
    'aliases' => array('pe'),
  );

  return $items;
}

/**
 * Enable the selected profile and it's depdencies.
 */
function drush_profile_enable_validate($profile) {
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
