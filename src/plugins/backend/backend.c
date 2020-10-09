

#include "backend.h"
#include "backendprivate.h"

#include <kdberrors.h>
#include <kdblogger.h>
#include <kdbprivate.h>

int setMountpoint (BackendHandle * bh, Key * root, KeySet * config, Key * errorKey)
{
	Key * configRoot;

	ksRewind (config);

	configRoot = ksNext (config);

	Key * searchMountpoint = keyDup (configRoot);
	keyAddBaseName (searchMountpoint, "mountpoint");
	Key * foundMountpoint = ksLookup (config, searchMountpoint, 0);

	keyDel (searchMountpoint);

	if (!foundMountpoint)
	{
		ELEKTRA_ADD_PLUGIN_MISBEHAVIOR_WARNINGF (errorKey, "Could not find mountpoint within root %s", keyName (configRoot));
		return -1;
	}

	bh->mountpoint = keyNew ("", KEY_VALUE, keyBaseName (root), KEY_END);
	elektraKeySetName (bh->mountpoint, keyString (foundMountpoint), KEY_CASCADING_NAME | KEY_EMPTY_NAME);

	keySetName (errorKey, keyName (bh->mountpoint));

	if (!bh->mountpoint)
	{
		ELEKTRA_ADD_PLUGIN_MISBEHAVIOR_WARNINGF (errorKey, "Could not create mountpoint with name '%s' and value %s",
							 keyString (foundMountpoint), keyBaseName (root));
		return -1;
	}

	return 0;
}

KeySet * processConfig (BackendHandle * bh, Key * root, KeySet * config, Key * errorKey)
{
	if (setMountpoint (bh, root, config, errorKey) == -1)
	{
		return NULL;
	}

	KeySet * systemConfig = ksRenameKeys (config, "system");
	ksDel (config);

	return systemConfig;
}

int linkedListPosition (Key * cur, Key * errorKey)
{
	char * name = elektraCalloc (strlen (keyBaseName (cur)) + 1);
	strcpy (name, keyBaseName (cur));

	if (name[0] != '#')
	{
		ELEKTRA_ADD_PLUGIN_MISBEHAVIOR_WARNINGF (errorKey, "Plugin positions must start with a #. Position: %s", name);
		return -1;
	}

	int numberOfDigits = 1;

	while (name[numberOfDigits] == '_')
	{
		numberOfDigits++;
	}

	int position = 0;

	int digitCountdown = numberOfDigits;

	while (digitCountdown != 0)
	{
		position *= 10;

		if (name[numberOfDigits] < '0' || name[numberOfDigits] > '9')
		{
			ELEKTRA_ADD_PLUGIN_MISBEHAVIOR_WARNINGF (errorKey, "Plugin positions must be numbers. Position: %s", name);
			return -1;
		}

		position += name[numberOfDigits] - '0';

		numberOfDigits++;
		digitCountdown--;
	}

	elektraFree (name);

	return position;
}

int processPlugin (KeySet * config, Key * cur, char ** name, KeySet ** pluginConfig, char ** referenceName, Key * errorKey)
{
	// This key will be used to find the configuration of the plugin, if it exists
	Key * pluginConfigSearchKey = keyDup (cur);
	keyAddBaseName (pluginConfigSearchKey, "config");

	// This key will be used to find the plugin label, which it can be referenced with afterwards, if it exists
	Key * labelSearchKey = keyDup (cur);
	keyAddBaseName (labelSearchKey, "label");

	// This key will be used to find the name of the plugin
	Key * nameSearchKey = keyDup (cur);
	keyAddBaseName (nameSearchKey, "name");

	// This key will be used to find the plugin's reference name, which is the label it was first set with
	Key * refSearchKey = keyDup (cur);
	keyAddBaseName (refSearchKey, "reference");

	KeySet * cutPluginConfig = ksCut (config, pluginConfigSearchKey);
	keyDel (pluginConfigSearchKey);

	*pluginConfig = ksRenameKeys (cutPluginConfig, "user");
	ksDel (cutPluginConfig);

	Key * labelKey = ksLookup (config, labelSearchKey, KDB_O_POP);

	Key * nameKey = ksLookup (config, nameSearchKey, KDB_O_POP);

	Key * refKey = ksLookup (config, refSearchKey, KDB_O_POP);

	keyDel (labelSearchKey);
	keyDel (nameSearchKey);
	keyDel (refSearchKey);

	if (refKey != 0)
	{
		// An existing plugin will be used
		if (labelKey != 0)
		{
			ELEKTRA_ADD_PLUGIN_MISBEHAVIOR_WARNINGF (
				errorKey,
				"The label and reference name of a plugin cannot be defined at the same time! Label: %s Reference name: %s",
				keyString (labelKey), keyString (refKey));
			return -1;
		}

		if (nameKey != 0)
		{
			ELEKTRA_ADD_PLUGIN_MISBEHAVIOR_WARNINGF (errorKey,
								 "The plugin name and reference name of a plugin cannot be defined at the "
								 "same time! Plugin name: %s, Reference name: %s",
								 keyString (nameKey), keyString (refKey));
			return -1;
		}

		char prefixReferenceName[] = "system/elektra/plugins/";
		char * keyName = elektraCalloc (strlen (keyString (refKey)) + 1);
		strcpy (keyName, keyString (refKey));

		*referenceName = elektraCalloc (strlen (prefixReferenceName) + strlen (keyName) + 1);
		strcpy (*referenceName, prefixReferenceName);
		strcat (*referenceName, keyName);

		elektraFree (keyName);

		return 2;
	}
	else if (nameKey != 0) // A new plugin will be created
	{
		*name = elektraCalloc (strlen (keyString (nameKey)) + 1);
		strcpy (*name, keyString (nameKey));
		if (labelKey != 0)
		{
			// A label will be defined for later referencing

			char prefixReferenceName[] = "system/elektra/plugins/";
			char * keyName = elektraCalloc (strlen (keyString (labelKey)) + 1);
			strcpy (keyName, keyString (labelKey));

			*referenceName = elektraCalloc (strlen (prefixReferenceName) + strlen (keyString (labelKey)) + 1);
			strcpy (*referenceName, prefixReferenceName);
			strcat (*referenceName, keyName);

			elektraFree (keyName);

			return 3;
		}
		return 1;
	}
	else if (labelKey != 0)
	{
		ELEKTRA_ADD_PLUGIN_MISBEHAVIOR_WARNINGF (
			errorKey, "The plugin name must be defined If the label is defined! Plugin name: %s, Label: %s",
			keyString (nameKey), keyString (labelKey));
		return -1;
	}
	else
	{
		ELEKTRA_ADD_PLUGIN_MISBEHAVIOR_WARNING (errorKey, "Plugin name, reference name and label are not defined!");
		return -1;
	}
}

Slot * processRole (KeySet * config, KeySet * modules, KeySet * referencePlugins, KeySet * systemConfig, Key * errorKey)
{
	Key * root;
	Key * cur;

	ksRewind (config);

	root = ksNext (config);

	Slot * slot = 0;

	while ((cur = ksNext (config)) != 0)
	{
		if (keyIsDirectlyBelow (root, cur) == 1)
		{
			int position;

			if ((position = linkedListPosition (cur, errorKey)) == -1)
			{
				ELEKTRA_ADD_PLUGIN_MISBEHAVIOR_WARNINGF (
					errorKey, "Could not parse the position of the plugin in the linked list from the key: %s",
					keyName (cur));
				return 0;
			}

			char * name = 0;

			KeySet * cut = ksCut (config, cur);

			KeySet * pluginConfig;

			char * referenceName = 0;

			int ret = processPlugin (cut, cur, &name, &pluginConfig, &referenceName, errorKey);

			ksDel (cut);

			if (ret == -1)
			{
				ELEKTRA_ADD_PLUGIN_MISBEHAVIOR_WARNING (errorKey, "Could not parse plugin name, label and configuration");
				return NULL;
			}

			if (!slot)
			{
				slot = elektraCalloc (sizeof (Slot));
				slot->next = 0;
			}

			Slot * curSlot = slot;

			for (int a = 0; a <= position; a++)
			{
				// Insert the plugin into its slot
				if (a == position)
				{
					if (name)
					{
						ksAppend (pluginConfig, systemConfig);
						ksRewind (pluginConfig);

						curSlot->value = elektraPluginOpen (name, modules, ksDup (pluginConfig), errorKey);

						if (!curSlot->value)
						{
							ELEKTRA_ADD_PLUGIN_MISBEHAVIOR_WARNINGF (errorKey, "Could not load plugin %s",
												 name);
							elektraFree (name);

							return 0;
						}

						if (referenceName)
						{
							ksAppendKey (referencePlugins,
								     keyNew (referenceName, KEY_BINARY, KEY_SIZE, sizeof (curSlot->value),
									     KEY_VALUE, &curSlot->value, KEY_END));
						}
					}
					else
					{
						Key * lookup = ksLookup (referencePlugins, keyNew (referenceName, KEY_END), KDB_O_DEL);
						if (!lookup)
						{
							ELEKTRA_ADD_PLUGIN_MISBEHAVIOR_WARNINGF (
								errorKey, "Could not reference back to plugin %s", referenceName);
							elektraFree (referenceName);
							ksDel (config);
							return 0;
						}

						curSlot->value = *(Plugin **) keyValue (lookup);
						++curSlot->value->refcounter;
					}
				}
				else
				{
					if (!curSlot->next)
					{
						curSlot->next = elektraCalloc (sizeof (Slot));
					}
					curSlot = curSlot->next;
				}
			}
			ksDel (pluginConfig);
			elektraFree (referenceName);
			elektraFree (name);
		}
		else
		{
			ELEKTRA_ADD_INSTALLATION_WARNINGF (errorKey, "Unknown entries in plugin configuration: %s", keyString (cur));
		}
	}

	ksDel (config);

	return slot;
}

int processGetPlugins (Slot ** slots, KeySet * modules, KeySet * referencePlugins, KeySet * config, KeySet * systemConfig, Key * errorKey)
{
	Key * root;
	Key * cur;

	ksRewind (config);

	root = ksNext (config);

	while ((cur = ksNext (config)))
	{
		if (keyIsDirectlyBelow (root, cur) == 1)
		{
			KeySet * cut = ksCut (config, cur);
			Slot * slot;
			if (!strcmp (keyBaseName (cur), "getresolver"))
			{
				slot = processRole (cut, modules, referencePlugins, systemConfig, errorKey);

				if (slot == 0)
				{
					ELEKTRA_ADD_PLUGIN_MISBEHAVIOR_WARNING (errorKey, "Could not build up slots for role: resolver");
					return -1;
				}

				slots[0] = slot;
			}
			else if (!strcmp (keyBaseName (cur), "pregetstorage"))
			{
				slot = processRole (cut, modules, referencePlugins, systemConfig, errorKey);

				if (slot == 0)
				{
					ELEKTRA_ADD_PLUGIN_MISBEHAVIOR_WARNING (errorKey,
										"Could not build up slots for role: pregetstorage");
					return -1;
				}

				slots[1] = slot;
			}
			else if (!strcmp (keyBaseName (cur), "getstorage"))
			{
				slot = processRole (cut, modules, referencePlugins, systemConfig, errorKey);

				if (slot == 0)
				{
					ELEKTRA_ADD_PLUGIN_MISBEHAVIOR_WARNING (errorKey, "Could not build up slots for role: getstorage");
					return -1;
				}

				slots[2] = slot;
			}
			else if (!strcmp (keyBaseName (cur), "postgetstorage"))
			{
				slot = processRole (cut, modules, referencePlugins, systemConfig, errorKey);

				if (slot == 0)
				{
					ELEKTRA_ADD_PLUGIN_MISBEHAVIOR_WARNING (errorKey,
										"Could not build up slots for role: postgetstorage");
					return -1;
				}

				slots[3] = slot;
			}
			else
			{
				ELEKTRA_ADD_PLUGIN_MISBEHAVIOR_WARNINGF (errorKey, "Unexpected key: %s", keyName (cur));
				return -1;
			}
		}
	}

	return 1;
}

int processSetPlugins (Slot ** slots, KeySet * modules, KeySet * referencePlugins, KeySet * config, KeySet * systemConfig, Key * errorKey)
{
	Key * root;
	Key * cur;

	ksRewind (config);

	root = ksNext (config);

	while ((cur = ksNext (config)) != 0)
	{
		if (keyIsDirectlyBelow (root, cur) == 1)
		{
			KeySet * cut = ksCut (config, cur);
			Slot * slot;
			if (!strcmp (keyBaseName (cur), "setresolver"))
			{
				slot = processRole (cut, modules, referencePlugins, systemConfig, errorKey);

				if (slot == 0)
				{
					ELEKTRA_ADD_PLUGIN_MISBEHAVIOR_WARNING (errorKey, "Could not build up slots for role: resolver");
					return -1;
				}

				slots[0] = slot;
			}
			else if (!strcmp (keyBaseName (cur), "presetstorage"))
			{
				slot = processRole (cut, modules, referencePlugins, systemConfig, errorKey);

				if (slot == 0)
				{
					ELEKTRA_ADD_PLUGIN_MISBEHAVIOR_WARNING (errorKey, "Could not build up slots for role: prestorage");
					return -1;
				}

				slots[1] = slot;
			}
			else if (!strcmp (keyBaseName (cur), "setstorage"))
			{
				slot = processRole (cut, modules, referencePlugins, systemConfig, errorKey);

				if (slot == 0)
				{
					ELEKTRA_ADD_PLUGIN_MISBEHAVIOR_WARNING (errorKey, "Could not build up slots for role: storage");
					return -1;
				}

				slots[2] = slot;
			}
			else if (!strcmp (keyBaseName (cur), "precommit"))
			{
				slot = processRole (cut, modules, referencePlugins, systemConfig, errorKey);

				if (slot == 0)
				{
					ELEKTRA_ADD_PLUGIN_MISBEHAVIOR_WARNING (errorKey, "Could not build up slots for role: precommit");
					return -1;
				}

				slots[3] = slot;
			}
			else if (!strcmp (keyBaseName (cur), "commit"))
			{
				slot = processRole (cut, modules, referencePlugins, systemConfig, errorKey);

				if (slot == 0)
				{
					ELEKTRA_ADD_PLUGIN_MISBEHAVIOR_WARNING (errorKey, "Could not build up slots for role: commit");
					return -1;
				}

				slots[4] = slot;
			}
			else if (!strcmp (keyBaseName (cur), "postcommit"))
			{
				slot = processRole (cut, modules, referencePlugins, systemConfig, errorKey);

				if (slot == 0)
				{
					ELEKTRA_ADD_PLUGIN_MISBEHAVIOR_WARNING (errorKey, "Could not build up slots for role: postcommit");
					return -1;
				}

				slots[5] = slot;
			}
			else
			{
				ELEKTRA_ADD_PLUGIN_MISBEHAVIOR_WARNINGF (errorKey, "Unexpected key: %s", keyName (cur));
				return -1;
			}
		}
	}

	return 1;
}

int processErrorPlugins (Slot ** slots, KeySet * modules, KeySet * referencePlugins, KeySet * config, KeySet * systemConfig, Key * errorKey)
{
	Key * root;
	Key * cur;

	ksRewind (config);

	root = ksNext (config);

	while ((cur = ksNext (config)) != 0)
	{
		if (keyIsDirectlyBelow (root, cur) == 1)
		{
			KeySet * cut = ksCut (config, cur);
			Slot * slot;
			if (!strcmp (keyBaseName (cur), "prerollback"))
			{
				slot = processRole (cut, modules, referencePlugins, systemConfig, errorKey);

				if (slot == 0)
				{
					ELEKTRA_ADD_PLUGIN_MISBEHAVIOR_WARNING (errorKey, "Could not build up prerollback slots");
					return -1;
				}

				slots[0] = slot;
			}
			else if (!strcmp (keyBaseName (cur), "rollback"))
			{
				slot = processRole (cut, modules, referencePlugins, systemConfig, errorKey);

				if (slot == 0)
				{
					ELEKTRA_ADD_PLUGIN_MISBEHAVIOR_WARNING (errorKey, "Could not build up rollback slots");
					return -1;
				}

				slots[1] = slot;
			}
			else if (!strcmp (keyBaseName (cur), "postrollback"))
			{
				slot = processRole (cut, modules, referencePlugins, systemConfig, errorKey);

				if (slot == 0)
				{
					ELEKTRA_ADD_PLUGIN_MISBEHAVIOR_WARNING (errorKey, "Could not build up postrollback slots");
					return -1;
				}

				slots[2] = slot;
			}
			else
			{
				ELEKTRA_ADD_PLUGIN_MISBEHAVIOR_WARNINGF (errorKey, "Unexpected key: %s", keyName (cur));
				return -1;
			}
		}
	}

	return 1;
}

int elektraBackendOpen (Plugin * handle, Key * errorKey)
{
	BackendHandle * bh = elektraCalloc (sizeof (BackendHandle));

	elektraPluginSetData (handle, bh);

	bh->getposition = GET_GETRESOLVER;
	bh->setposition = SET_SETRESOLVER;
	bh->errorposition = ERROR_PREROLLBACK;

	ksRewind (handle->config);

	Key * root = ksNext (handle->config);

	KeySet * systemConfig = 0;
	KeySet * referencePlugins = ksNew (0, KS_END);

	Key * configKey = keyDup (root);
	keyAddBaseName (configKey, "config");
	KeySet * configSet = ksCut (handle->config, configKey);
	keyDel (configKey);

	systemConfig = processConfig (bh, root, configSet, errorKey);

	if (systemConfig == 0)
	{
		ELEKTRA_ADD_PLUGIN_MISBEHAVIOR_WARNING (errorKey, "Could not generate system config");
		return ELEKTRA_PLUGIN_STATUS_ERROR;
	}

	Key * errorPluginsKey = keyDup (root);
	keyAddBaseName (errorPluginsKey, "error");
	KeySet * errorPluginsSet = ksCut (handle->config, errorPluginsKey);
	keyDel (errorPluginsKey);

	Slot ** errorPlugins = elektraCalloc ((sizeof (Slot *)) * NR_OF_ERROR_PLUGINS);

	if (processErrorPlugins (errorPlugins, handle->modules, referencePlugins, errorPluginsSet, systemConfig, errorKey) == -1)
	{
		ELEKTRA_ADD_PLUGIN_MISBEHAVIOR_WARNING (errorKey, "Could not build up error array");
		return ELEKTRA_PLUGIN_STATUS_ERROR;
	}

	ksDel (errorPluginsSet);

	for (int a = 0; a < NR_OF_ERROR_PLUGINS; a++)
	{
		bh->errorplugins[a] = errorPlugins[a];
	}

	elektraFree (errorPlugins);

	Key * getPluginsKey = keyDup (root);
	keyAddBaseName (getPluginsKey, "get");
	KeySet * getPluginsSet = ksCut (handle->config, getPluginsKey);
	keyDel (getPluginsKey);

	Slot ** getPlugins = elektraCalloc ((sizeof (Slot *)) * NR_OF_GET_PLUGINS);

	if (processGetPlugins (getPlugins, handle->modules, referencePlugins, getPluginsSet, systemConfig, errorKey) == -1)
	{
		ELEKTRA_ADD_PLUGIN_MISBEHAVIOR_WARNING (errorKey, "Could not build up get array");
		return ELEKTRA_PLUGIN_STATUS_ERROR;
	}

	ksDel (getPluginsSet);

	for (int a = 0; a < NR_OF_GET_PLUGINS; a++)
	{
		bh->getplugins[a] = getPlugins[a];
	}

	elektraFree (getPlugins);

	Key * setPluginsKey = keyDup (root);
	keyAddBaseName (setPluginsKey, "set");
	KeySet * setPluginsSet = ksCut (handle->config, setPluginsKey);
	keyDel (setPluginsKey);

	Slot ** setPlugins = elektraCalloc ((sizeof (Slot *)) * NR_OF_SET_PLUGINS);

	if (processSetPlugins (setPlugins, handle->modules, referencePlugins, setPluginsSet, systemConfig, errorKey) == -1)
	{
		ELEKTRA_ADD_PLUGIN_MISBEHAVIOR_WARNING (errorKey, "Could not build up set array");
		return ELEKTRA_PLUGIN_STATUS_ERROR;
	}

	ksDel (setPluginsSet);

	for (int a = 0; a < NR_OF_SET_PLUGINS; a++)
	{
		bh->setplugins[a] = setPlugins[a];
	}
	elektraFree (setPlugins);

	ksDel (referencePlugins);

	handle->config = systemConfig;

	// TODO Open missing backend instead of returning errors

	return ELEKTRA_PLUGIN_STATUS_SUCCESS;
}

int elektraBackendClose (Plugin * handle, Key * errorKey)
{
	if (!handle)
	{
		return ELEKTRA_PLUGIN_STATUS_ERROR;
	}

	int error = 0;

	BackendHandle * bh = elektraPluginGetData (handle);
	if (!bh)
	{
		return ELEKTRA_PLUGIN_STATUS_ERROR;
	}

	keyDel (bh->mountpoint);

	for (int a = 0; a < NR_OF_GET_PLUGINS; a++)
	{
		Slot * cur = bh->getplugins[a];
		if (!cur)
		{
			continue;
		}

		while (cur)
		{
			int ret = elektraPluginClose (cur->value, errorKey);
			if (ret == -1)
			{
				++error;
			}
			Slot * prev = cur;
			cur = cur->next;
			elektraFree (prev);
		}
	}

	for (int a = 0; a < NR_OF_SET_PLUGINS; a++)
	{
		Slot * cur = bh->setplugins[a];
		if (!cur)
		{
			continue;
		}

		while (cur)
		{
			int ret = elektraPluginClose (cur->value, errorKey);
			if (ret == -1)
			{
				++error;
			}
			Slot * prev = cur;
			cur = cur->next;
			elektraFree (prev);
		}
	}

	for (int a = 0; a < NR_OF_ERROR_PLUGINS; a++)
	{
		Slot * cur = bh->errorplugins[a];
		if (!cur)
		{
			continue;
		}

		while (cur)
		{
			int ret = elektraPluginClose (cur->value, errorKey);
			if (ret == -1)
			{
				++error;
			}
			Slot * prev = cur;
			cur = cur->next;
			elektraFree (prev);
		}
	}

	if (bh)
	{
		elektraFree (bh);
		elektraPluginSetData (handle, 0);
	}

	if (error)
	{
		return ELEKTRA_PLUGIN_STATUS_ERROR;
	}

	return ELEKTRA_PLUGIN_STATUS_SUCCESS;
}

void incrementGetPosition (BackendHandle * bh)
{
	if (bh->getposition == GET_POSTGETSTORAGE)
	{
		bh->getposition = GET_GETRESOLVER;
	}
	else
	{
		bh->getposition++;
	}
}

void incrementSetPosition (BackendHandle * bh)
{
	if (bh->setposition == SET_POSTCOMMIT)
	{
		bh->setposition = SET_SETRESOLVER;
	}
	else
	{
		bh->setposition++;
	}
}

void incrementErrorPosition (BackendHandle * bh)
{
	if (bh->errorposition == ERROR_POSTROLLBACK)
	{
		bh->errorposition = ERROR_PREROLLBACK;
	}
	else
	{
		bh->errorposition++;
	}
}

int elektraBackendGet (Plugin * handle, KeySet * ks, Key * parentKey)
{
	BackendHandle * bh = elektraPluginGetData (handle);

	if (!bh)
	{
		ELEKTRA_ADD_PLUGIN_MISBEHAVIOR_WARNING (parentKey, "The backend handle is not defined!");
		return ELEKTRA_PLUGIN_STATUS_ERROR;
	}

	if (bh->getposition > GET_POSTGETSTORAGE || bh->getposition < GET_GETRESOLVER)
	{
		ELEKTRA_ADD_PLUGIN_MISBEHAVIOR_WARNING (parentKey, "Invalid plugin position!");
		return ELEKTRA_PLUGIN_STATUS_ERROR;
	}

	if (bh->getposition == GET_GETRESOLVER)
	{
		Slot * resolver = bh->getplugins[GET_GETRESOLVER];

		if (!resolver || !resolver->value)
		{
			ELEKTRA_LOG ("No resolver, continuing");

			incrementGetPosition (bh);
			return ELEKTRA_PLUGIN_STATUS_SUCCESS;
		}

		if (!resolver || !resolver->value || !resolver->value->kdbGet)
		{
			ELEKTRA_ADD_PLUGIN_MISBEHAVIOR_WARNING (
				parentKey, "The resolver is not defined properly, the backend was not initialized correctly!");
			return ELEKTRA_PLUGIN_STATUS_ERROR;
		}

		int returnValue = resolver->value->kdbGet (resolver->value, ks, parentKey);

		switch (returnValue)
		{
		case ELEKTRA_PLUGIN_STATUS_CACHE_HIT:
			return returnValue;
		case ELEKTRA_PLUGIN_STATUS_NO_UPDATE:
			return returnValue;
		case ELEKTRA_PLUGIN_STATUS_ERROR:
			return returnValue;
		case ELEKTRA_PLUGIN_STATUS_SUCCESS:
			incrementGetPosition (bh);
			return returnValue;
		default:
			ELEKTRA_ADD_PLUGIN_MISBEHAVIOR_WARNING (parentKey, "Invalid resolver return value!");
			return ELEKTRA_PLUGIN_STATUS_ERROR;
		}
	}

	if (bh->getplugins[bh->getposition])
	{
		Slot * cur = bh->getplugins[bh->getposition];

		while (cur != 0)
		{
			if (cur->value && cur->value->kdbGet)
			{
				if (cur->value->kdbGet (cur->value, ks, parentKey) == ELEKTRA_PLUGIN_STATUS_ERROR)
				{
					incrementGetPosition (bh);
					return ELEKTRA_PLUGIN_STATUS_ERROR;
				}

				if (bh->getposition == GET_GETRESOLVER || bh->getposition == GET_GETSTORAGE)
				{
					cur = 0;
				}
				else
				{
					cur = cur->next;
				}
			}
		}
	}

	incrementGetPosition (bh);

	return ELEKTRA_PLUGIN_STATUS_SUCCESS;
}

int elektraBackendSet (Plugin * handle, KeySet * ks, Key * parentKey)
{
	// TODO implement global plugins
	BackendHandle * bh = elektraPluginGetData (handle);
	if (bh == NULL)
	{
		ELEKTRA_ADD_PLUGIN_MISBEHAVIOR_WARNING (parentKey, "The backend handle is not defined correctly!");
		return ELEKTRA_PLUGIN_STATUS_ERROR;
	}

	if (bh->setposition < SET_SETRESOLVER || bh->setposition > SET_POSTCOMMIT)
	{
		ELEKTRA_ADD_PLUGIN_MISBEHAVIOR_WARNING (parentKey, "Invalid plugin position!");
		return ELEKTRA_PLUGIN_STATUS_ERROR;
	}

	if (bh->setposition == SET_SETRESOLVER)
	{
		Slot * resolver = bh->setplugins[SET_SETRESOLVER];
		if (!resolver || !resolver->value)
		{
			ELEKTRA_ADD_PLUGIN_MISBEHAVIOR_WARNING (parentKey, "The resolver plugin is not defined!");
			return ELEKTRA_PLUGIN_STATUS_ERROR;
		}
		if (!resolver->value->kdbSet)
		{
			ELEKTRA_ADD_PLUGIN_MISBEHAVIOR_WARNING (parentKey, "The set function of the resolver plugin is not defined!");
			return ELEKTRA_PLUGIN_STATUS_ERROR;
		}

		int ret = resolver->value->kdbSet (resolver->value, ks, parentKey);

		switch (ret)
		{
		case ELEKTRA_PLUGIN_STATUS_NO_UPDATE:
			return ret;
		case ELEKTRA_PLUGIN_STATUS_ERROR:
			return ret;
		case ELEKTRA_PLUGIN_STATUS_SUCCESS:
			incrementSetPosition (bh);
			return ret;
		default:
			ELEKTRA_ADD_PLUGIN_MISBEHAVIOR_WARNING (parentKey, "Invalid resolver return value!");
			return ELEKTRA_PLUGIN_STATUS_ERROR;
		}
	}

	if (bh->setplugins[bh->setposition])
	{
		Slot * cur = bh->setplugins[bh->setposition];
		while (cur != 0)
		{
			if (cur->value && cur->value->kdbSet)
			{
				if (cur->value->kdbSet (cur->value, ks, parentKey) == ELEKTRA_PLUGIN_STATUS_ERROR)
				{
					ELEKTRA_ADD_PLUGIN_MISBEHAVIOR_WARNINGF (parentKey,
										 "Error while carrying out kdbSet before the commit: %s",
										 keyName (bh->mountpoint));
					incrementSetPosition (bh);

					return ELEKTRA_PLUGIN_STATUS_ERROR;
				}
			}
			cur = cur->next;
		}
	}

	incrementSetPosition (bh);

	return ELEKTRA_PLUGIN_STATUS_SUCCESS;
}

int elektraBackendCommit (Plugin * handle, KeySet * ks, Key * parentKey)
{
	BackendHandle * bh = elektraPluginGetData (handle);
	if (bh == NULL)
	{
		ELEKTRA_ADD_PLUGIN_MISBEHAVIOR_WARNING (parentKey, "The backend handle is not defined correctly!");
		return ELEKTRA_PLUGIN_STATUS_ERROR;
	}

	if (bh->setposition != SET_COMMIT)
	{
		ELEKTRA_ADD_PLUGIN_MISBEHAVIOR_WARNING (parentKey, "Invalid plugin position!");
		return ELEKTRA_PLUGIN_STATUS_ERROR;
	}

	if (bh->setplugins[SET_COMMIT] && bh->setplugins[SET_COMMIT]->value && bh->setplugins[SET_COMMIT]->value->kdbCommit)
	{
		if (bh->setplugins[SET_COMMIT]->value->kdbCommit (bh->setplugins[SET_COMMIT]->value, ks, parentKey) ==
		    ELEKTRA_PLUGIN_STATUS_ERROR)
		{
			ELEKTRA_ADD_PLUGIN_MISBEHAVIOR_WARNING (parentKey, "Error during the commit phase");
		}
	}

	incrementSetPosition (bh);

	return ELEKTRA_PLUGIN_STATUS_SUCCESS;
}

int elektraBackendError (Plugin * handle, KeySet * ks, Key * parentKey)
{
	BackendHandle * bh = elektraPluginGetData (handle);
	if (bh == NULL)
	{
		ELEKTRA_ADD_PLUGIN_MISBEHAVIOR_WARNING (parentKey, "The backend handle is not defined correctly!");
		return ELEKTRA_PLUGIN_STATUS_ERROR;
	}

	if (bh->errorposition < ERROR_PREROLLBACK || bh->errorposition > ERROR_POSTROLLBACK)
	{
		ELEKTRA_ADD_PLUGIN_MISBEHAVIOR_WARNING (parentKey, "Invalid plugin position!");
		return ELEKTRA_PLUGIN_STATUS_ERROR;
	}

	Slot * cur = bh->errorplugins[bh->errorposition];
	while (cur != 0)
	{
		if (cur->value && cur->value->kdbError)
		{
			if (cur->value->kdbError (cur->value, ks, parentKey) == ELEKTRA_PLUGIN_STATUS_ERROR)
			{
				ELEKTRA_ADD_PLUGIN_MISBEHAVIOR_WARNING (parentKey, "Error while carrying out kdbError(oh the irony)");
			}
		}
		cur = cur->next;
	}

	incrementErrorPosition (bh);

	return ELEKTRA_PLUGIN_STATUS_SUCCESS;
}

Plugin * ELEKTRA_PLUGIN_EXPORT
{
	return elektraPluginExport ("backend", ELEKTRA_PLUGIN_OPEN, &elektraBackendOpen, ELEKTRA_PLUGIN_CLOSE, &elektraBackendClose,
				    ELEKTRA_PLUGIN_GET, &elektraBackendGet, ELEKTRA_PLUGIN_SET, &elektraBackendSet, ELEKTRA_PLUGIN_COMMIT,
				    &elektraBackendCommit, ELEKTRA_PLUGIN_ERROR, &elektraBackendError, ELEKTRA_PLUGIN_END);
}