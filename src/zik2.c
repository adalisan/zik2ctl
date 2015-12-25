/* Zik2ctl
 * Copyright (C) 2015 Aurélien Zanelli <aurelien.zanelli@darkosphere.fr>
 *
 * Zik2ctl is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Zik2ctl is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with Zik2ctl. If not, see <http://www.gnu.org/licenses/>.
 */

#include <string.h>

#include "zik2.h"
#include "zik2connection.h"
#include "zik2message.h"
#include "zik2info.h"

#define UNKNOWN_STR "unknown"
#define DEFAULT_NOISE_CONTROL_STRENGTH 1

enum
{
  PROP_0,
  PROP_NAME,
  PROP_ADDRESS,
  PROP_SERIAL,
  PROP_SOFTWARE_VERSION,
  PROP_NOISE_CONTROL_ENABLED,
  PROP_NOISE_CONTROL_MODE,
  PROP_NOISE_CONTROL_STRENGTH,
  PROP_SOURCE,
  PROP_BATTERY_STATE,
  PROP_BATTERY_PERCENT,
};

#define ZIK2_NOISE_CONTROL_MODE_TYPE (zik2_noise_control_mode_get_type ())
static GType
zik2_noise_control_mode_get_type (void)
{
  static volatile GType type;

  static const GEnumValue modes[] = {
    { ZIK2_NOISE_CONTROL_MODE_OFF, "Disable noise control", "off" },
    { ZIK2_NOISE_CONTROL_MODE_ANC, "Enable noise cancelling", "anc" },
    { ZIK2_NOISE_CONTROL_MODE_AOC, "Enable street mode", "aoc" },
    { 0, NULL, NULL }
  };

  if (g_once_init_enter (&type)) {
    GType _type;

    _type = g_enum_register_static ("Zik2NoiseControlMode", modes);

    g_once_init_leave (&type, _type);
  }

  return type;
}

#define parent_class zik2_parent_class
G_DEFINE_TYPE (Zik2, zik2, G_TYPE_OBJECT);

/* GObject methods */
static void zik2_finalize (GObject * object);
static void zik2_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec *pspec);
static void zik2_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec *pspec);

static void
zik2_class_init (Zik2Class * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = zik2_finalize;
  gobject_class->get_property = zik2_get_property;
  gobject_class->set_property = zik2_set_property;

  g_object_class_install_property (gobject_class, PROP_NAME,
      g_param_spec_string ("name", "Name", "Zik2 name", UNKNOWN_STR,
          G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_ADDRESS,
      g_param_spec_string ("address", "Address", "Zik2 bluetooth address",
          UNKNOWN_STR,
          G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_SERIAL,
      g_param_spec_string ("serial", "Serial", "Zik2 serial number",
          UNKNOWN_STR, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_SOFTWARE_VERSION,
      g_param_spec_string ("software-version", "Software-version",
          "Zik2 software version", UNKNOWN_STR,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_SOURCE,
      g_param_spec_string ("source", "Source", "Zik2 audio source",
          UNKNOWN_STR, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_NOISE_CONTROL_ENABLED,
      g_param_spec_boolean ("noise-control-enabled", "Noise control enabled",
          "Zik2 noise control enabled status", FALSE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_NOISE_CONTROL_MODE,
      g_param_spec_enum ("noise-control-mode", "Noise control mode",
          "Select the noise control mode", ZIK2_NOISE_CONTROL_MODE_TYPE,
          ZIK2_NOISE_CONTROL_MODE_OFF,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_NOISE_CONTROL_STRENGTH,
      g_param_spec_uint ("noise-control-strength", "Noise control strength",
        "Set the noise control strength", 1, 2, DEFAULT_NOISE_CONTROL_STRENGTH,
        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_BATTERY_STATE,
      g_param_spec_string ("battery-state", "Battery state",
        "State of the battery", UNKNOWN_STR,
        G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_BATTERY_PERCENT,
      g_param_spec_uint ("battery-percentage", "Battery percentage",
        "Battery charge percentage", 0, 100, 0,
        G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
}

static void
zik2_init (Zik2 * zik2)
{
  zik2->serial = g_strdup (UNKNOWN_STR);
  zik2->software_version = g_strdup (UNKNOWN_STR);
  zik2->source = g_strdup (UNKNOWN_STR);
  zik2->battery_state = g_strdup (UNKNOWN_STR);

  zik2->noise_control_strength = DEFAULT_NOISE_CONTROL_STRENGTH;
}

static void
zik2_finalize (GObject * object)
{
  Zik2 *zik2 = ZIK2 (object);

  g_free (zik2->name);
  g_free (zik2->address);
  g_free (zik2->serial);
  g_free (zik2->software_version);
  g_free (zik2->source);
  g_free (zik2->battery_state);

  if (zik2->conn)
    zik2_connection_free (zik2->conn);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static gboolean
zik2_get_and_parse_reply (Zik2 * zik2, const gchar * path,
    Zik2RequestReplyData ** reply)
{
  Zik2Message *msg;
  Zik2Message *answer = NULL;
  Zik2RequestReplyData *result;
  gboolean ret = FALSE;

  msg = zik2_message_new_request (path, "get", NULL);

  if (!zik2_connection_send_message (zik2->conn, msg, &answer)) {
    g_critical ("failed to send get request '%s/get'", path);
    goto out;
  }

  if (!zik2_message_parse_request_reply (answer, &result)) {
    g_critical ("failed to parse request reply for '%s/get'", path);
    goto out;
  }

  if (zik2_request_reply_data_error (result)) {
    g_warning ("device reply with error for '%s/get'", path);
    zik2_request_reply_data_free (result);
    goto out;
  }

  *reply = result;
  ret = TRUE;

out:
  zik2_message_free (msg);

  if (answer)
    zik2_message_free (answer);

  return ret;
}

static void
zik2_get_serial (Zik2 * zik2)
{
  Zik2RequestReplyData *reply = NULL;
  Zik2SystemInfo *info;

  if (!zik2_get_and_parse_reply (zik2, API_SYSTEM_PI_URI, &reply)) {
    g_warning ("failed to get serial");
    goto out;
  }

  info = zik2_request_reply_data_find_node_info (reply, ZIK2_SYSTEM_INFO_TYPE);
  if (info == NULL) {
    g_warning ("<system> not found");
    goto out;
  }

  g_free (zik2->serial);
  zik2->serial = g_strdup (info->pi);

out:
  if (reply)
    zik2_request_reply_data_free (reply);
}

static void
zik2_get_noise_control (Zik2 * zik2)
{
  Zik2RequestReplyData *reply = NULL;
  Zik2NoiseControlInfo *info;

  if (!zik2_get_and_parse_reply (zik2, API_AUDIO_NOISE_CONTROL_ENABLED_URI,
        &reply)) {
    g_warning ("failed to get noise control status");
    goto out;
  }

  info = zik2_request_reply_data_find_node_info (reply,
      ZIK2_NOISE_CONTROL_INFO_TYPE);
  if (info == NULL) {
    g_warning ("<noise_control> not found");
    goto out;
  }

  zik2->noise_control_enabled = info->enabled;

out:
  if (reply)
    zik2_request_reply_data_free (reply);
}

static gboolean
zik2_set_noise_control (Zik2 * zik2, gboolean active)
{
  Zik2Message *msg;
  gboolean ret;

  msg = zik2_message_new_request (API_AUDIO_NOISE_CONTROL_ENABLED_URI, "set",
      active ? "true" : "false");
  ret = zik2_connection_send_message (zik2->conn, msg, NULL);
  zik2_message_free (msg);

  return ret;
}

static void
zik2_get_noise_control_mode_and_strength (Zik2 * zik2)
{
  Zik2RequestReplyData *reply = NULL;
  Zik2NoiseControlInfo *info;
  GEnumClass *klass;
  GEnumValue *mode;

  if (!zik2_get_and_parse_reply (zik2, API_AUDIO_NOISE_CONTROL_URI,
        &reply)) {
    g_warning ("failed to get noise control status");
    goto out;
  }

  info = zik2_request_reply_data_find_node_info (reply,
      ZIK2_NOISE_CONTROL_INFO_TYPE);
  if (info == NULL) {
    g_warning ("<noise_control> not found");
    goto out;
  }

  klass = G_ENUM_CLASS (g_type_class_peek (ZIK2_NOISE_CONTROL_MODE_TYPE));
  mode = g_enum_get_value_by_nick (klass, info->type);
  if (mode == NULL) {
    g_warning ("failed to get enum value associated with '%s'", info->type);
    goto out;
  }
  zik2->noise_control_mode = mode->value;
  zik2->noise_control_strength = info->value;

out:
  if (reply)
    zik2_request_reply_data_free (reply);
}

static gboolean
zik2_set_noise_control_mode_and_strength (Zik2 * zik2,
    Zik2NoiseControlMode mode, guint strength)
{
  Zik2Message *msg;
  gboolean ret;
  const gchar *type;
  gchar *args;

  switch (mode) {
    case ZIK2_NOISE_CONTROL_MODE_OFF:
      type = "off";
      break;
    case ZIK2_NOISE_CONTROL_MODE_ANC:
      type = "anc";
      break;
    case ZIK2_NOISE_CONTROL_MODE_AOC:
      type = "aoc";
      break;
    default:
      g_assert_not_reached ();
  }

  args = g_strdup_printf ("%s&value=%u", type, strength);
  msg = zik2_message_new_request (API_AUDIO_NOISE_CONTROL_URI, "set", args);
  g_free (args);

  ret = zik2_connection_send_message (zik2->conn, msg, NULL);
  zik2_message_free (msg);

  return ret;
}

static void
zik2_get_software_version (Zik2 * zik2)
{
  Zik2RequestReplyData *reply = NULL;
  Zik2SoftwareInfo *info;

  if (!zik2_get_and_parse_reply (zik2, API_SOFTWARE_VERSION_URI, &reply)) {
    g_warning ("failed to get software");
    goto out;
  }

  info = zik2_request_reply_data_find_node_info (reply,
      ZIK2_SOFTWARE_INFO_TYPE);
  if (info == NULL) {
    g_warning ("<software> not found");
    goto out;
  }

  g_free (zik2->software_version);
  zik2->software_version = g_strdup (info->sip6);

out:
  if (reply)
    zik2_request_reply_data_free (reply);
}

static void
zik2_get_source (Zik2 * zik2)
{
  Zik2RequestReplyData *reply = NULL;
  Zik2SourceInfo *info;

  if (!zik2_get_and_parse_reply (zik2, API_AUDIO_SOURCE_URI, &reply)) {
    g_warning ("failed to get audio source");
    goto out;
  }

  info = zik2_request_reply_data_find_node_info (reply, ZIK2_SOURCE_INFO_TYPE);
  if (info == NULL) {
    g_warning ("<source> not found");
    goto out;
  }

  g_free (zik2->source);
  zik2->source = g_strdup (info->type);

out:
  if (reply)
    zik2_request_reply_data_free (reply);
}

static void
zik2_get_battery (Zik2 * zik2)
{
  Zik2RequestReplyData *reply = NULL;
  Zik2BatteryInfo *info;

  if (!zik2_get_and_parse_reply (zik2, API_SYSTEM_BATTERY_URI, &reply)) {
    g_warning ("failed to get system battery");
    goto out;
  }

  info = zik2_request_reply_data_find_node_info (reply, ZIK2_BATTERY_INFO_TYPE);
  if (info == NULL) {
    g_warning ("<battery> not found");
    goto out;
  }

  g_free (zik2->battery_state);
  zik2->battery_state = g_strdup (info->state);
  zik2->battery_percentage = info->percent;

out:
  if (reply)
    zik2_request_reply_data_free (reply);
}

static void
zik2_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec *pspec)
{
  Zik2 *zik2 = ZIK2 (object);

  switch (prop_id) {
    case PROP_NAME:
      g_value_set_string (value, zik2->name);
      break;
    case PROP_ADDRESS:
      g_value_set_string (value, zik2->address);
      break;
    case PROP_SERIAL:
      g_value_set_string (value, zik2->serial);
      break;
    case PROP_SOFTWARE_VERSION:
      g_value_set_string (value, zik2->software_version);
      break;
    case PROP_SOURCE:
      zik2_get_source (zik2);
      g_value_set_string (value, zik2->source);
      break;
    case PROP_NOISE_CONTROL_ENABLED:
      g_value_set_boolean (value, zik2->noise_control_enabled);
      break;
    case PROP_NOISE_CONTROL_MODE:
      g_value_set_enum (value, zik2->noise_control_mode);
      break;
    case PROP_NOISE_CONTROL_STRENGTH:
      g_value_set_uint (value, zik2->noise_control_strength);
      break;
    case PROP_BATTERY_STATE:
      zik2_get_battery (zik2);
      g_value_set_string (value, zik2->battery_state);
      break;
    case PROP_BATTERY_PERCENT:
      zik2_get_battery (zik2);
      g_value_set_uint (value, zik2->battery_percentage);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
zik2_set_property (GObject * object, guint prop_id, const GValue * value,
    GParamSpec *pspec)
{
  Zik2 *zik2 = ZIK2 (object);

  switch (prop_id) {
    case PROP_NAME:
      zik2->name = g_value_dup_string (value);
      break;
    case PROP_ADDRESS:
      zik2->address = g_value_dup_string (value);
      break;
    case PROP_NOISE_CONTROL_ENABLED:
      {
        gboolean tmp;

        tmp = g_value_get_boolean (value);
        if (zik2_set_noise_control (zik2, tmp))
          zik2->noise_control_enabled = tmp;
        else
          g_warning ("failed to set noise control enabled");
      }
      break;
    case PROP_NOISE_CONTROL_MODE:
      {
        Zik2NoiseControlMode tmp;

        tmp = g_value_get_enum (value);
        if (zik2_set_noise_control_mode_and_strength (zik2, tmp,
              zik2->noise_control_strength))
          zik2->noise_control_mode = tmp;
        else
           g_warning ("failed to set noise control mode");
      }
      break;
    case PROP_NOISE_CONTROL_STRENGTH:
      {
        guint tmp;

        tmp = g_value_get_uint (value);
        if (zik2_set_noise_control_mode_and_strength (zik2,
              zik2->noise_control_mode, tmp))
          zik2->noise_control_strength = tmp;
        else
          g_warning ("failed to set noise control strength");
      }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/* @conn: (transfer full) */
Zik2 *
zik2_new (const gchar * name, const gchar * address, Zik2Connection * conn)
{
  Zik2 *zik2;

  zik2 = g_object_new (ZIK2_TYPE, "name", name, "address", address, NULL);
  zik2->conn = conn;

  /* sync with devices */
  zik2_get_serial (zik2);
  zik2_get_noise_control (zik2);
  zik2_get_noise_control_mode_and_strength (zik2);
  zik2_get_software_version (zik2);
  zik2_get_source (zik2);

  return zik2;
}
