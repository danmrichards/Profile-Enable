INTRODUCTION
------------

FIRST THINGS FIRST: THIS IS NOT A MODULE. PLEASE READ BELOW.

'Profile Enable' is an extension for Drush. This extension provides a new Drush
command, 'profile-enable', which is designed for the enabling and re-enabling of
Drupal installation profiles. Now I know that Drush itself can handle this
perfectly well using 'pm-enable' and installation profiles are modules. But I
do want to make you aware of a special use case that required the development
of 'Profile Enable'.

Drupal by default does not support inheritance on installation profiles in the
same way that it does for themes. This is a highly sought after feature and the
Drupal community has 2 issue threads dedicated to it:

* https://www.drupal.org/node/2067229 (D7)
* https://www.drupal.org/node/1356276 (D8)

This extension allows you to enable an installation profile using the Drush
command. The command will then correctly evaluate all the dependencies from the
inherited installation profiles, as defined by 'base profile' in the profile
.info files. I expect profile inheritance to make it into core in D8 (or
maybe 8.1), which will hopefully mean a backport for D7.

Currently 'Profile Enable' only supports D7.

REQUIREMENTS
------------

* A Drupal 7 installation
* Drush (Preferably version 8+) - https://www.drupal.org/project/drush

OPTIONAL
--------

* Patch applied from https://www.drupal.org/node/2067229 for inherited profile
  support.

INSTALLATION
------------

* Copy the 'profile_enable' folder to sites/all/drush in your Drupal doc root.
* Clear the Drush cache by running 'drush cc drush' from your command line.

USAGE
----

* From your Drupal root run 'drush profile-enable PROFILE_NAME'.

Where PROFILE_NAME is the machine name of a Drupal installation profile. For the
lazy there is also an alias for the command - 'drush pe PROFILE_NAME'.

MAINTAINERS
-----------
Current maintainers:
  * Dan Richards - https://www.drupal.org/user/3157375

This project has been sponsored by:
  * LUSH Digital - https://www.drupal.org/node/2529922
    In order for us to become a company of the future and clear cosmetic leader
    we are putting the internet at the heart of our business. It's time for Lush
    to be at the forefront of digital and revolutionise the cosmetics industry.

    Lush Digital enables technology to connect people throughout our community
    and build relationships bringing customer to the heart of our business.
