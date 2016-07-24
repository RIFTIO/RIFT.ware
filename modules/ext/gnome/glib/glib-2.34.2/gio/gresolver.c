/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright (C) 2008 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"
#include <glib.h>
#include "glibintl.h"

#include "gresolver.h"
#include "gnetworkingprivate.h"
#include "gasyncresult.h"
#include "ginetaddress.h"
#include "ginetsocketaddress.h"
#include "gsimpleasyncresult.h"
#include "gsrvtarget.h"
#include "gthreadedresolver.h"

#ifdef G_OS_UNIX
#include <sys/stat.h>
#endif

#include <stdlib.h>


/**
 * SECTION:gresolver
 * @short_description: Asynchronous and cancellable DNS resolver
 * @include: gio/gio.h
 *
 * #GResolver provides cancellable synchronous and asynchronous DNS
 * resolution, for hostnames (g_resolver_lookup_by_address(),
 * g_resolver_lookup_by_name() and their async variants) and SRV
 * (service) records (g_resolver_lookup_service()).
 *
 * #GNetworkAddress and #GNetworkService provide wrappers around
 * #GResolver functionality that also implement #GSocketConnectable,
 * making it easy to connect to a remote host/service.
 */

enum {
  RELOAD,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

struct _GResolverPrivate {
#ifdef G_OS_UNIX
  time_t resolv_conf_timestamp;
#else
  int dummy;
#endif
};

/**
 * GResolver:
 *
 * The object that handles DNS resolution. Use g_resolver_get_default()
 * to get the default resolver.
 */
G_DEFINE_TYPE (GResolver, g_resolver, G_TYPE_OBJECT)

static GList *
srv_records_to_targets (GList *records)
{
  const gchar *hostname;
  guint16 port, priority, weight;
  GSrvTarget *target;
  GList *l;

  for (l = records; l != NULL; l = g_list_next (l))
    {
      g_variant_get (l->data, "(qqq&s)", &priority, &weight, &port, &hostname);
      target = g_srv_target_new (hostname, port, priority, weight);
      g_variant_unref (l->data);
      l->data = target;
    }

  return g_srv_target_list_sort (records);
}

static GList *
g_resolver_real_lookup_service (GResolver            *resolver,
                                const gchar          *rrname,
                                GCancellable         *cancellable,
                                GError              **error)
{
  GList *records;

  records = G_RESOLVER_GET_CLASS (resolver)->lookup_records (resolver,
                                                             rrname,
                                                             G_RESOLVER_RECORD_SRV,
                                                             cancellable,
                                                             error);

  return srv_records_to_targets (records);
}

static void
g_resolver_real_lookup_service_async (GResolver            *resolver,
                                      const gchar          *rrname,
                                      GCancellable         *cancellable,
                                      GAsyncReadyCallback   callback,
                                      gpointer              user_data)
{
  G_RESOLVER_GET_CLASS (resolver)->lookup_records_async (resolver,
                                                         rrname,
                                                         G_RESOLVER_RECORD_SRV,
                                                         cancellable,
                                                         callback,
                                                         user_data);
}

static GList *
g_resolver_real_lookup_service_finish (GResolver            *resolver,
                                       GAsyncResult         *result,
                                       GError              **error)
{
  GList *records;

  records = G_RESOLVER_GET_CLASS (resolver)->lookup_records_finish (resolver,
                                                                    result,
                                                                    error);

  return srv_records_to_targets (records);
}

static void
g_resolver_class_init (GResolverClass *resolver_class)
{
  /* Automatically pass these over to the lookup_records methods */
  resolver_class->lookup_service = g_resolver_real_lookup_service;
  resolver_class->lookup_service_async = g_resolver_real_lookup_service_async;
  resolver_class->lookup_service_finish = g_resolver_real_lookup_service_finish;

  g_type_class_add_private (resolver_class, sizeof (GResolverPrivate));

  /* Make sure _g_networking_init() has been called */
  g_type_ensure (G_TYPE_INET_ADDRESS);

  /* Initialize _g_resolver_addrinfo_hints */
#ifdef AI_ADDRCONFIG
  _g_resolver_addrinfo_hints.ai_flags |= AI_ADDRCONFIG;
#endif
  /* These two don't actually matter, they just get copied into the
   * returned addrinfo structures (and then we ignore them). But if
   * we leave them unset, we'll get back duplicate answers.
   */
  _g_resolver_addrinfo_hints.ai_socktype = SOCK_STREAM;
  _g_resolver_addrinfo_hints.ai_protocol = IPPROTO_TCP;

  /**
   * GResolver::reload:
   * @resolver: a #GResolver
   *
   * Emitted when the resolver notices that the system resolver
   * configuration has changed.
   **/
  signals[RELOAD] =
    g_signal_new (I_("reload"),
		  G_TYPE_RESOLVER,
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GResolverClass, reload),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
}

static void
g_resolver_init (GResolver *resolver)
{
#ifdef G_OS_UNIX
  struct stat st;
#endif

  resolver->priv = G_TYPE_INSTANCE_GET_PRIVATE (resolver, G_TYPE_RESOLVER, GResolverPrivate);

#ifdef G_OS_UNIX
  if (stat (_PATH_RESCONF, &st) == 0)
    resolver->priv->resolv_conf_timestamp = st.st_mtime;
#endif
}

static GResolver *default_resolver;

/**
 * g_resolver_get_default:
 *
 * Gets the default #GResolver. You should unref it when you are done
 * with it. #GResolver may use its reference count as a hint about how
 * many threads it should allocate for concurrent DNS resolutions.
 *
 * Return value: (transfer full): the default #GResolver.
 *
 * Since: 2.22
 */
GResolver *
g_resolver_get_default (void)
{
  if (!default_resolver)
    default_resolver = g_object_new (G_TYPE_THREADED_RESOLVER, NULL);

  return g_object_ref (default_resolver);
}

/**
 * g_resolver_set_default:
 * @resolver: the new default #GResolver
 *
 * Sets @resolver to be the application's default resolver (reffing
 * @resolver, and unreffing the previous default resolver, if any).
 * Future calls to g_resolver_get_default() will return this resolver.
 *
 * This can be used if an application wants to perform any sort of DNS
 * caching or "pinning"; it can implement its own #GResolver that
 * calls the original default resolver for DNS operations, and
 * implements its own cache policies on top of that, and then set
 * itself as the default resolver for all later code to use.
 *
 * Since: 2.22
 */
void
g_resolver_set_default (GResolver *resolver)
{
  if (default_resolver)
    g_object_unref (default_resolver);
  default_resolver = g_object_ref (resolver);
}


static void
g_resolver_maybe_reload (GResolver *resolver)
{
#ifdef G_OS_UNIX
  struct stat st;

  if (stat (_PATH_RESCONF, &st) == 0)
    {
      if (st.st_mtime != resolver->priv->resolv_conf_timestamp)
        {
          resolver->priv->resolv_conf_timestamp = st.st_mtime;
          res_init ();
          g_signal_emit (resolver, signals[RELOAD], 0);
        }
    }
#endif
}

/* filter out duplicates, cf. https://bugzilla.gnome.org/show_bug.cgi?id=631379 */
static void
remove_duplicates (GList *addrs)
{
  GList *l;
  GList *ll;
  GList *lll;

  /* TODO: if this is too slow (it's O(n^2) but n is typically really
   * small), we can do something more clever but note that we must not
   * change the order of elements...
   */
  for (l = addrs; l != NULL; l = l->next)
    {
      GInetAddress *address = G_INET_ADDRESS (l->data);
      for (ll = l->next; ll != NULL; ll = lll)
        {
          GInetAddress *other_address = G_INET_ADDRESS (ll->data);
          lll = ll->next;
          if (g_inet_address_equal (address, other_address))
            {
              g_object_unref (other_address);
              /* we never return the first element */
              g_warn_if_fail (g_list_delete_link (addrs, ll) == addrs);
            }
        }
    }
}


/**
 * g_resolver_lookup_by_name:
 * @resolver: a #GResolver
 * @hostname: the hostname to look up
 * @cancellable: (allow-none): a #GCancellable, or %NULL
 * @error: return location for a #GError, or %NULL
 *
 * Synchronously resolves @hostname to determine its associated IP
 * address(es). @hostname may be an ASCII-only or UTF-8 hostname, or
 * the textual form of an IP address (in which case this just becomes
 * a wrapper around g_inet_address_new_from_string()).
 *
 * On success, g_resolver_lookup_by_name() will return a #GList of
 * #GInetAddress, sorted in order of preference and guaranteed to not
 * contain duplicates. That is, if using the result to connect to
 * @hostname, you should attempt to connect to the first address
 * first, then the second if the first fails, etc. If you are using
 * the result to listen on a socket, it is appropriate to add each
 * result using e.g. g_socket_listener_add_address().
 *
 * If the DNS resolution fails, @error (if non-%NULL) will be set to a
 * value from #GResolverError.
 *
 * If @cancellable is non-%NULL, it can be used to cancel the
 * operation, in which case @error (if non-%NULL) will be set to
 * %G_IO_ERROR_CANCELLED.
 *
 * If you are planning to connect to a socket on the resolved IP
 * address, it may be easier to create a #GNetworkAddress and use its
 * #GSocketConnectable interface.
 *
 * Return value: (element-type GInetAddress) (transfer full): a #GList
 * of #GInetAddress, or %NULL on error. You
 * must unref each of the addresses and free the list when you are
 * done with it. (You can use g_resolver_free_addresses() to do this.)
 *
 * Since: 2.22
 */
GList *
g_resolver_lookup_by_name (GResolver     *resolver,
                           const gchar   *hostname,
                           GCancellable  *cancellable,
                           GError       **error)
{
  GInetAddress *addr;
  GList *addrs;
  gchar *ascii_hostname = NULL;

  g_return_val_if_fail (G_IS_RESOLVER (resolver), NULL);
  g_return_val_if_fail (hostname != NULL, NULL);

  /* Check if @hostname is just an IP address */
  addr = g_inet_address_new_from_string (hostname);
  if (addr)
    return g_list_append (NULL, addr);

  if (g_hostname_is_non_ascii (hostname))
    hostname = ascii_hostname = g_hostname_to_ascii (hostname);

  g_resolver_maybe_reload (resolver);
  addrs = G_RESOLVER_GET_CLASS (resolver)->
    lookup_by_name (resolver, hostname, cancellable, error);

  remove_duplicates (addrs);

  g_free (ascii_hostname);
  return addrs;
}

/**
 * g_resolver_lookup_by_name_async:
 * @resolver: a #GResolver
 * @hostname: the hostname to look up the address of
 * @cancellable: (allow-none): a #GCancellable, or %NULL
 * @callback: (scope async): callback to call after resolution completes
 * @user_data: (closure): data for @callback
 *
 * Begins asynchronously resolving @hostname to determine its
 * associated IP address(es), and eventually calls @callback, which
 * must call g_resolver_lookup_by_name_finish() to get the result.
 * See g_resolver_lookup_by_name() for more details.
 *
 * Since: 2.22
 */
void
g_resolver_lookup_by_name_async (GResolver           *resolver,
                                 const gchar         *hostname,
                                 GCancellable        *cancellable,
                                 GAsyncReadyCallback  callback,
                                 gpointer             user_data)
{
  GInetAddress *addr;
  gchar *ascii_hostname = NULL;

  g_return_if_fail (G_IS_RESOLVER (resolver));
  g_return_if_fail (hostname != NULL);

  /* Check if @hostname is just an IP address */
  addr = g_inet_address_new_from_string (hostname);
  if (addr)
    {
      GSimpleAsyncResult *simple;

      simple = g_simple_async_result_new (G_OBJECT (resolver),
                                          callback, user_data,
                                          g_resolver_lookup_by_name_async);

      g_simple_async_result_set_op_res_gpointer (simple, addr, g_object_unref);
      g_simple_async_result_complete_in_idle (simple);
      g_object_unref (simple);
      return;
    }

  if (g_hostname_is_non_ascii (hostname))
    hostname = ascii_hostname = g_hostname_to_ascii (hostname);

  g_resolver_maybe_reload (resolver);
  G_RESOLVER_GET_CLASS (resolver)->
    lookup_by_name_async (resolver, hostname, cancellable, callback, user_data);

  g_free (ascii_hostname);
}

/**
 * g_resolver_lookup_by_name_finish:
 * @resolver: a #GResolver
 * @result: the result passed to your #GAsyncReadyCallback
 * @error: return location for a #GError, or %NULL
 *
 * Retrieves the result of a call to
 * g_resolver_lookup_by_name_async().
 *
 * If the DNS resolution failed, @error (if non-%NULL) will be set to
 * a value from #GResolverError. If the operation was cancelled,
 * @error will be set to %G_IO_ERROR_CANCELLED.
 *
 * Return value: (element-type GInetAddress) (transfer full): a #GList
 * of #GInetAddress, or %NULL on error. See g_resolver_lookup_by_name()
 * for more details.
 *
 * Since: 2.22
 */
GList *
g_resolver_lookup_by_name_finish (GResolver     *resolver,
                                  GAsyncResult  *result,
                                  GError       **error)
{
  GList *addrs;

  g_return_val_if_fail (G_IS_RESOLVER (resolver), NULL);

  if (g_async_result_legacy_propagate_error (result, error))
    return NULL;
  else if (g_async_result_is_tagged (result, g_resolver_lookup_by_name_async))
    {
      GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (result);
      GInetAddress *addr;

      /* Handle the stringified-IP-addr case */
      addr = g_simple_async_result_get_op_res_gpointer (simple);
      return g_list_append (NULL, g_object_ref (addr));
    }

  addrs = G_RESOLVER_GET_CLASS (resolver)->
    lookup_by_name_finish (resolver, result, error);

  remove_duplicates (addrs);

  return addrs;
}

/**
 * g_resolver_free_addresses: (skip)
 * @addresses: a #GList of #GInetAddress
 *
 * Frees @addresses (which should be the return value from
 * g_resolver_lookup_by_name() or g_resolver_lookup_by_name_finish()).
 * (This is a convenience method; you can also simply free the results
 * by hand.)
 *
 * Since: 2.22
 */
void
g_resolver_free_addresses (GList *addresses)
{
  GList *a;

  for (a = addresses; a; a = a->next)
    g_object_unref (a->data);
  g_list_free (addresses);
}

/**
 * g_resolver_lookup_by_address:
 * @resolver: a #GResolver
 * @address: the address to reverse-resolve
 * @cancellable: (allow-none): a #GCancellable, or %NULL
 * @error: return location for a #GError, or %NULL
 *
 * Synchronously reverse-resolves @address to determine its
 * associated hostname.
 *
 * If the DNS resolution fails, @error (if non-%NULL) will be set to
 * a value from #GResolverError.
 *
 * If @cancellable is non-%NULL, it can be used to cancel the
 * operation, in which case @error (if non-%NULL) will be set to
 * %G_IO_ERROR_CANCELLED.
 *
 * Return value: a hostname (either ASCII-only, or in ASCII-encoded
 *     form), or %NULL on error.
 *
 * Since: 2.22
 */
gchar *
g_resolver_lookup_by_address (GResolver     *resolver,
                              GInetAddress  *address,
                              GCancellable  *cancellable,
                              GError       **error)
{
  g_return_val_if_fail (G_IS_RESOLVER (resolver), NULL);
  g_return_val_if_fail (G_IS_INET_ADDRESS (address), NULL);

  g_resolver_maybe_reload (resolver);
  return G_RESOLVER_GET_CLASS (resolver)->
    lookup_by_address (resolver, address, cancellable, error);
}

/**
 * g_resolver_lookup_by_address_async:
 * @resolver: a #GResolver
 * @address: the address to reverse-resolve
 * @cancellable: (allow-none): a #GCancellable, or %NULL
 * @callback: (scope async): callback to call after resolution completes
 * @user_data: (closure): data for @callback
 *
 * Begins asynchronously reverse-resolving @address to determine its
 * associated hostname, and eventually calls @callback, which must
 * call g_resolver_lookup_by_address_finish() to get the final result.
 *
 * Since: 2.22
 */
void
g_resolver_lookup_by_address_async (GResolver           *resolver,
                                    GInetAddress        *address,
                                    GCancellable        *cancellable,
                                    GAsyncReadyCallback  callback,
                                    gpointer             user_data)
{
  g_return_if_fail (G_IS_RESOLVER (resolver));
  g_return_if_fail (G_IS_INET_ADDRESS (address));

  g_resolver_maybe_reload (resolver);
  G_RESOLVER_GET_CLASS (resolver)->
    lookup_by_address_async (resolver, address, cancellable, callback, user_data);
}

/**
 * g_resolver_lookup_by_address_finish:
 * @resolver: a #GResolver
 * @result: the result passed to your #GAsyncReadyCallback
 * @error: return location for a #GError, or %NULL
 *
 * Retrieves the result of a previous call to
 * g_resolver_lookup_by_address_async().
 *
 * If the DNS resolution failed, @error (if non-%NULL) will be set to
 * a value from #GResolverError. If the operation was cancelled,
 * @error will be set to %G_IO_ERROR_CANCELLED.
 *
 * Return value: a hostname (either ASCII-only, or in ASCII-encoded
 * form), or %NULL on error.
 *
 * Since: 2.22
 */
gchar *
g_resolver_lookup_by_address_finish (GResolver     *resolver,
                                     GAsyncResult  *result,
                                     GError       **error)
{
  g_return_val_if_fail (G_IS_RESOLVER (resolver), NULL);

  if (g_async_result_legacy_propagate_error (result, error))
    return NULL;

  return G_RESOLVER_GET_CLASS (resolver)->
    lookup_by_address_finish (resolver, result, error);
}

static gchar *
g_resolver_get_service_rrname (const char *service,
                               const char *protocol,
                               const char *domain)
{
  gchar *rrname, *ascii_domain = NULL;

  if (g_hostname_is_non_ascii (domain))
    domain = ascii_domain = g_hostname_to_ascii (domain);

  rrname = g_strdup_printf ("_%s._%s.%s", service, protocol, domain);

  g_free (ascii_domain);
  return rrname;
}

/**
 * g_resolver_lookup_service:
 * @resolver: a #GResolver
 * @service: the service type to look up (eg, "ldap")
 * @protocol: the networking protocol to use for @service (eg, "tcp")
 * @domain: the DNS domain to look up the service in
 * @cancellable: (allow-none): a #GCancellable, or %NULL
 * @error: return location for a #GError, or %NULL
 *
 * Synchronously performs a DNS SRV lookup for the given @service and
 * @protocol in the given @domain and returns an array of #GSrvTarget.
 * @domain may be an ASCII-only or UTF-8 hostname. Note also that the
 * @service and @protocol arguments <emphasis>do not</emphasis>
 * include the leading underscore that appears in the actual DNS
 * entry.
 *
 * On success, g_resolver_lookup_service() will return a #GList of
 * #GSrvTarget, sorted in order of preference. (That is, you should
 * attempt to connect to the first target first, then the second if
 * the first fails, etc.)
 *
 * If the DNS resolution fails, @error (if non-%NULL) will be set to
 * a value from #GResolverError.
 *
 * If @cancellable is non-%NULL, it can be used to cancel the
 * operation, in which case @error (if non-%NULL) will be set to
 * %G_IO_ERROR_CANCELLED.
 *
 * If you are planning to connect to the service, it is usually easier
 * to create a #GNetworkService and use its #GSocketConnectable
 * interface.
 *
 * Return value: (element-type GSrvTarget) (transfer full): a #GList of #GSrvTarget,
 * or %NULL on error. You must free each of the targets and the list when you are
 * done with it. (You can use g_resolver_free_targets() to do this.)
 *
 * Since: 2.22
 */
GList *
g_resolver_lookup_service (GResolver     *resolver,
                           const gchar   *service,
                           const gchar   *protocol,
                           const gchar   *domain,
                           GCancellable  *cancellable,
                           GError       **error)
{
  GList *targets;
  gchar *rrname;

  g_return_val_if_fail (G_IS_RESOLVER (resolver), NULL);
  g_return_val_if_fail (service != NULL, NULL);
  g_return_val_if_fail (protocol != NULL, NULL);
  g_return_val_if_fail (domain != NULL, NULL);

  rrname = g_resolver_get_service_rrname (service, protocol, domain);

  g_resolver_maybe_reload (resolver);
  targets = G_RESOLVER_GET_CLASS (resolver)->
    lookup_service (resolver, rrname, cancellable, error);

  g_free (rrname);
  return targets;
}

/**
 * g_resolver_lookup_service_async:
 * @resolver: a #GResolver
 * @service: the service type to look up (eg, "ldap")
 * @protocol: the networking protocol to use for @service (eg, "tcp")
 * @domain: the DNS domain to look up the service in
 * @cancellable: (allow-none): a #GCancellable, or %NULL
 * @callback: (scope async): callback to call after resolution completes
 * @user_data: (closure): data for @callback
 *
 * Begins asynchronously performing a DNS SRV lookup for the given
 * @service and @protocol in the given @domain, and eventually calls
 * @callback, which must call g_resolver_lookup_service_finish() to
 * get the final result. See g_resolver_lookup_service() for more
 * details.
 *
 * Since: 2.22
 */
void
g_resolver_lookup_service_async (GResolver           *resolver,
                                 const gchar         *service,
                                 const gchar         *protocol,
                                 const gchar         *domain,
                                 GCancellable        *cancellable,
                                 GAsyncReadyCallback  callback,
                                 gpointer             user_data)
{
  gchar *rrname;

  g_return_if_fail (G_IS_RESOLVER (resolver));
  g_return_if_fail (service != NULL);
  g_return_if_fail (protocol != NULL);
  g_return_if_fail (domain != NULL);

  rrname = g_resolver_get_service_rrname (service, protocol, domain);

  g_resolver_maybe_reload (resolver);
  G_RESOLVER_GET_CLASS (resolver)->
    lookup_service_async (resolver, rrname, cancellable, callback, user_data);

  g_free (rrname);
}

/**
 * g_resolver_lookup_service_finish:
 * @resolver: a #GResolver
 * @result: the result passed to your #GAsyncReadyCallback
 * @error: return location for a #GError, or %NULL
 *
 * Retrieves the result of a previous call to
 * g_resolver_lookup_service_async().
 *
 * If the DNS resolution failed, @error (if non-%NULL) will be set to
 * a value from #GResolverError. If the operation was cancelled,
 * @error will be set to %G_IO_ERROR_CANCELLED.
 *
 * Return value: (element-type GSrvTarget) (transfer full): a #GList of #GSrvTarget,
 * or %NULL on error. See g_resolver_lookup_service() for more details.
 *
 * Since: 2.22
 */
GList *
g_resolver_lookup_service_finish (GResolver     *resolver,
                                  GAsyncResult  *result,
                                  GError       **error)
{
  g_return_val_if_fail (G_IS_RESOLVER (resolver), NULL);

  if (g_async_result_legacy_propagate_error (result, error))
    return NULL;

  return G_RESOLVER_GET_CLASS (resolver)->
    lookup_service_finish (resolver, result, error);
}

/**
 * g_resolver_free_targets: (skip)
 * @targets: a #GList of #GSrvTarget
 *
 * Frees @targets (which should be the return value from
 * g_resolver_lookup_service() or g_resolver_lookup_service_finish()).
 * (This is a convenience method; you can also simply free the
 * results by hand.)
 *
 * Since: 2.22
 */
void
g_resolver_free_targets (GList *targets)
{
  GList *t;

  for (t = targets; t; t = t->next)
    g_srv_target_free (t->data);
  g_list_free (targets);
}

/**
 * g_resolver_lookup_records:
 * @resolver: a #GResolver
 * @rrname: the DNS name to lookup the record for
 * @record_type: the type of DNS record to lookup
 * @cancellable: (allow-none): a #GCancellable, or %NULL
 * @error: return location for a #GError, or %NULL
 *
 * Synchronously performs a DNS record lookup for the given @rrname and returns
 * a list of records as #GVariant tuples. See #GResolverRecordType for
 * information on what the records contain for each @record_type.
 *
 * If the DNS resolution fails, @error (if non-%NULL) will be set to
 * a value from #GResolverError.
 *
 * If @cancellable is non-%NULL, it can be used to cancel the
 * operation, in which case @error (if non-%NULL) will be set to
 * %G_IO_ERROR_CANCELLED.
 *
 * Return value: (element-type GVariant) (transfer full): a #GList of #GVariant,
 * or %NULL on error. You must free each of the records and the list when you are
 * done with it. (You can use g_list_free_full() with g_variant_unref() to do this.)
 *
 * Since: 2.34
 */
GList *
g_resolver_lookup_records (GResolver            *resolver,
                           const gchar          *rrname,
                           GResolverRecordType   record_type,
                           GCancellable         *cancellable,
                           GError              **error)
{
  GList *records;

  g_return_val_if_fail (G_IS_RESOLVER (resolver), NULL);
  g_return_val_if_fail (rrname != NULL, NULL);

  g_resolver_maybe_reload (resolver);
  records = G_RESOLVER_GET_CLASS (resolver)->
    lookup_records (resolver, rrname, record_type, cancellable, error);

  return records;
}

/**
 * g_resolver_lookup_records_async:
 * @resolver: a #GResolver
 * @rrname: the DNS name to lookup the record for
 * @record_type: the type of DNS record to lookup
 * @cancellable: (allow-none): a #GCancellable, or %NULL
 * @callback: (scope async): callback to call after resolution completes
 * @user_data: (closure): data for @callback
 *
 * Begins asynchronously performing a DNS lookup for the given
 * @rrname, and eventually calls @callback, which must call
 * g_resolver_lookup_records_finish() to get the final result. See
 * g_resolver_lookup_records() for more details.
 *
 * Since: 2.34
 */
void
g_resolver_lookup_records_async (GResolver           *resolver,
                                 const gchar         *rrname,
                                 GResolverRecordType  record_type,
                                 GCancellable        *cancellable,
                                 GAsyncReadyCallback  callback,
                                 gpointer             user_data)
{
  g_return_if_fail (G_IS_RESOLVER (resolver));
  g_return_if_fail (rrname != NULL);

  g_resolver_maybe_reload (resolver);
  G_RESOLVER_GET_CLASS (resolver)->
    lookup_records_async (resolver, rrname, record_type, cancellable, callback, user_data);
}

/**
 * g_resolver_lookup_records_finish:
 * @resolver: a #GResolver
 * @result: the result passed to your #GAsyncReadyCallback
 * @error: return location for a #GError, or %NULL
 *
 * Retrieves the result of a previous call to
 * g_resolver_lookup_records_async(). Returns a list of records as #GVariant
 * tuples. See #GResolverRecordType for information on what the records contain.
 *
 * If the DNS resolution failed, @error (if non-%NULL) will be set to
 * a value from #GResolverError. If the operation was cancelled,
 * @error will be set to %G_IO_ERROR_CANCELLED.
 *
 * Return value: (element-type GVariant) (transfer full): a #GList of #GVariant,
 * or %NULL on error. You must free each of the records and the list when you are
 * done with it. (You can use g_list_free_full() with g_variant_unref() to do this.)
 *
 * Since: 2.34
 */
GList *
g_resolver_lookup_records_finish (GResolver     *resolver,
                                  GAsyncResult  *result,
                                  GError       **error)
{
  g_return_val_if_fail (G_IS_RESOLVER (resolver), NULL);
  return G_RESOLVER_GET_CLASS (resolver)->
    lookup_records_finish (resolver, result, error);
}

/**
 * g_resolver_error_quark:
 *
 * Gets the #GResolver Error Quark.
 *
 * Return value: a #GQuark.
 *
 * Since: 2.22
 */
G_DEFINE_QUARK (g-resolver-error-quark, g_resolver_error)

static GResolverError
g_resolver_error_from_addrinfo_error (gint err)
{
  switch (err)
    {
    case EAI_FAIL:
#if defined(EAI_NODATA) && (EAI_NODATA != EAI_NONAME)
    case EAI_NODATA:
#endif
    case EAI_NONAME:
      return G_RESOLVER_ERROR_NOT_FOUND;

    case EAI_AGAIN:
      return G_RESOLVER_ERROR_TEMPORARY_FAILURE;

    default:
      return G_RESOLVER_ERROR_INTERNAL;
    }
}

struct addrinfo _g_resolver_addrinfo_hints;

/* Private method to process a getaddrinfo() response. */
GList *
_g_resolver_addresses_from_addrinfo (const char       *hostname,
                                     struct addrinfo  *res,
                                     gint              gai_retval,
                                     GError          **error)
{
  struct addrinfo *ai;
  GSocketAddress *sockaddr;
  GInetAddress *addr;
  GList *addrs;

  if (gai_retval != 0)
    {
      g_set_error (error, G_RESOLVER_ERROR,
		   g_resolver_error_from_addrinfo_error (gai_retval),
		   _("Error resolving '%s': %s"),
		   hostname, gai_strerror (gai_retval));
      return NULL;
    }

  g_return_val_if_fail (res != NULL, NULL);

  addrs = NULL;
  for (ai = res; ai; ai = ai->ai_next)
    {
      sockaddr = g_socket_address_new_from_native (ai->ai_addr, ai->ai_addrlen);
      if (!sockaddr || !G_IS_INET_SOCKET_ADDRESS (sockaddr))
        continue;

      addr = g_object_ref (g_inet_socket_address_get_address ((GInetSocketAddress *)sockaddr));
      addrs = g_list_prepend (addrs, addr);
      g_object_unref (sockaddr);
    }

  return g_list_reverse (addrs);
}

/* Private method to set up a getnameinfo() request */
void
_g_resolver_address_to_sockaddr (GInetAddress            *address,
                                 struct sockaddr_storage *sa,
                                 gsize                   *len)
{
  GSocketAddress *sockaddr;

  sockaddr = g_inet_socket_address_new (address, 0);
  g_socket_address_to_native (sockaddr, (struct sockaddr *)sa, sizeof (*sa), NULL);
  *len = g_socket_address_get_native_size (sockaddr);
  g_object_unref (sockaddr);
}

/* Private method to process a getnameinfo() response. */
char *
_g_resolver_name_from_nameinfo (GInetAddress  *address,
                                const gchar   *name,
                                gint           gni_retval,
                                GError       **error)
{
  if (gni_retval != 0)
    {
      gchar *phys;

      phys = g_inet_address_to_string (address);
      g_set_error (error, G_RESOLVER_ERROR,
                   g_resolver_error_from_addrinfo_error (gni_retval),
                   _("Error reverse-resolving '%s': %s"),
                   phys ? phys : "(unknown)", gai_strerror (gni_retval));
      g_free (phys);
      return NULL;
    }

  return g_strdup (name);
}

#if defined(G_OS_UNIX)

static gboolean
parse_short (guchar  **p,
             guchar   *end,
             guint16  *value)
{
  if (*p + 2 > end)
    return FALSE;
  GETSHORT (*value, *p);
  return TRUE;
}

static gboolean
parse_long (guchar  **p,
            guchar   *end,
            guint32  *value)
{
  if (*p + 4 > end)
    return FALSE;
  GETLONG (*value, *p);
  return TRUE;
}

static GVariant *
parse_res_srv (guchar  *answer,
               guchar  *end,
               guchar  *p)
{
  gchar namebuf[1024];
  guint16 priority, weight, port;
  gint n;

  if (!parse_short (&p, end, &priority) ||
      !parse_short (&p, end, &weight) ||
      !parse_short (&p, end, &port))
    return NULL;

  n = dn_expand (answer, end, p, namebuf, sizeof (namebuf));
  if (n < 0)
    return NULL;
  *p += n;

  return g_variant_new ("(qqqs)",
                        priority,
                        weight,
                        port,
                        namebuf);
}

static GVariant *
parse_res_soa (guchar  *answer,
               guchar  *end,
               guchar  *p)
{
  gchar mnamebuf[1024];
  gchar rnamebuf[1024];
  guint32 serial, refresh, retry, expire, ttl;
  gint n;

  n = dn_expand (answer, end, p, mnamebuf, sizeof (mnamebuf));
  if (n < 0)
    return NULL;
  p += n;

  n = dn_expand (answer, end, p, rnamebuf, sizeof (rnamebuf));
  if (n < 0)
    return NULL;
  p += n;

  if (!parse_long (&p, end, &serial) ||
      !parse_long (&p, end, &refresh) ||
      !parse_long (&p, end, &retry) ||
      !parse_long (&p, end, &expire) ||
      !parse_long (&p, end, &ttl))
    return NULL;

  return g_variant_new ("(ssuuuuu)",
                        mnamebuf,
                        rnamebuf,
                        serial,
                        refresh,
                        retry,
                        expire,
                        ttl);
}

static GVariant *
parse_res_ns (guchar  *answer,
              guchar  *end,
              guchar  *p)
{
  gchar namebuf[1024];
  gint n;

  n = dn_expand (answer, end, p, namebuf, sizeof (namebuf));
  if (n < 0)
    return NULL;

  return g_variant_new ("(s)", namebuf);
}

static GVariant *
parse_res_mx (guchar  *answer,
              guchar  *end,
              guchar  *p)
{
  gchar namebuf[1024];
  guint16 preference;
  gint n;

  if (!parse_short (&p, end, &preference))
    return NULL;

  n = dn_expand (answer, end, p, namebuf, sizeof (namebuf));
  if (n < 0)
    return NULL;
  p += n;

  return g_variant_new ("(qs)",
                        preference,
                        namebuf);
}

static GVariant *
parse_res_txt (guchar  *answer,
               guchar  *end,
               guchar  *p)
{
  GVariant *record;
  GPtrArray *array;
  gsize len;

  array = g_ptr_array_new_with_free_func (g_free);
  while (p < end)
    {
      len = *(p++);
      if (len > p - end)
        break;
      g_ptr_array_add (array, g_strndup ((gchar *)p, len));
      p += len;
    }

  record = g_variant_new ("(@as)",
                          g_variant_new_strv ((const gchar **)array->pdata, array->len));
  g_ptr_array_free (array, TRUE);
  return record;
}

gint
_g_resolver_record_type_to_rrtype (GResolverRecordType type)
{
  switch (type)
  {
    case G_RESOLVER_RECORD_SRV:
      return T_SRV;
    case G_RESOLVER_RECORD_TXT:
      return T_TXT;
    case G_RESOLVER_RECORD_SOA:
      return T_SOA;
    case G_RESOLVER_RECORD_NS:
      return T_NS;
    case G_RESOLVER_RECORD_MX:
      return T_MX;
  }
  g_return_val_if_reached (-1);
}

/* Private method to process a res_query response into GSrvTargets */
GList *
_g_resolver_records_from_res_query (const gchar      *rrname,
                                    gint              rrtype,
                                    guchar           *answer,
                                    gint              len,
                                    gint              herr,
                                    GError          **error)
{
  gint count;
  guchar *end, *p;
  guint16 type, qclass, rdlength;
  guint32 ttl;
  HEADER *header;
  GList *records;
  GVariant *record;
  gint n, i;

  if (len <= 0)
    {
      GResolverError errnum;
      const gchar *format;

      if (len == 0 || herr == HOST_NOT_FOUND || herr == NO_DATA)
        {
          errnum = G_RESOLVER_ERROR_NOT_FOUND;
          format = _("No DNS record of the requested type for '%s'");
        }
      else if (herr == TRY_AGAIN)
        {
          errnum = G_RESOLVER_ERROR_TEMPORARY_FAILURE;
          format = _("Temporarily unable to resolve '%s'");
        }
      else
        {
          errnum = G_RESOLVER_ERROR_INTERNAL;
          format = _("Error resolving '%s'");
        }

      g_set_error (error, G_RESOLVER_ERROR, errnum, format, rrname);
      return NULL;
    }

  records = NULL;

  header = (HEADER *)answer;
  p = answer + sizeof (HEADER);
  end = answer + len;

  /* Skip query */
  count = ntohs (header->qdcount);
  for (i = 0; i < count && p < end; i++)
    {
      n = dn_skipname (p, end);
      if (n < 0)
        break;
      p += n;
      p += 4;
    }

  /* Incomplete response */
  if (i < count)
    {
      g_set_error (error, G_RESOLVER_ERROR, G_RESOLVER_ERROR_TEMPORARY_FAILURE,
                   _("Incomplete data received for '%s'"), rrname);
      return NULL;
    }

  /* Read answers */
  count = ntohs (header->ancount);
  for (i = 0; i < count && p < end; i++)
    {
      n = dn_skipname (p, end);
      if (n < 0)
        break;
      p += n;

      if (!parse_short (&p, end, &type) ||
          !parse_short (&p, end, &qclass) ||
          !parse_long (&p, end, &ttl) ||
          !parse_short (&p, end, &rdlength))
        break;

      ttl = ttl; /* To avoid -Wunused-but-set-variable */

      if (p + rdlength > end)
        break;

      if (type == rrtype && qclass == C_IN)
        {
          switch (rrtype)
            {
            case T_SRV:
              record = parse_res_srv (answer, end, p);
              break;
            case T_MX:
              record = parse_res_mx (answer, end, p);
              break;
            case T_SOA:
              record = parse_res_soa (answer, end, p);
              break;
            case T_NS:
              record = parse_res_ns (answer, end, p);
              break;
            case T_TXT:
              record = parse_res_txt (answer, p + rdlength, p);
              break;
            default:
              g_warn_if_reached ();
              record = NULL;
              break;
            }

          if (record != NULL)
            records = g_list_prepend (records, record);
        }

      p += rdlength;
    }

  /* Somehow got a truncated response */
  if (i < count)
    {
      g_list_free_full (records, (GDestroyNotify)g_variant_unref);
      g_set_error (error, G_RESOLVER_ERROR, G_RESOLVER_ERROR_TEMPORARY_FAILURE,
                   _("Incomplete data received for '%s'"), rrname);
      return NULL;
    }

  return records;
}

#elif defined(G_OS_WIN32)
static GVariant *
parse_dns_srv (DNS_RECORD *rec)
{
  return g_variant_new ("(qqqs)",
                        (guint16)rec->Data.SRV.wPriority,
                        (guint16)rec->Data.SRV.wWeight,
                        (guint16)rec->Data.SRV.wPort,
                        rec->Data.SRV.pNameTarget);
}

static GVariant *
parse_dns_soa (DNS_RECORD *rec)
{
  return g_variant_new ("(ssuuuuu)",
                        rec->Data.SOA.pNamePrimaryServer,
                        rec->Data.SOA.pNameAdministrator,
                        (guint32)rec->Data.SOA.dwSerialNo,
                        (guint32)rec->Data.SOA.dwRefresh,
                        (guint32)rec->Data.SOA.dwRetry,
                        (guint32)rec->Data.SOA.dwExpire,
                        (guint32)rec->Data.SOA.dwDefaultTtl);
}

static GVariant *
parse_dns_ns (DNS_RECORD *rec)
{
  return g_variant_new ("(s)", rec->Data.NS.pNameHost);
}

static GVariant *
parse_dns_mx (DNS_RECORD *rec)
{
  return g_variant_new ("(qs)",
                        (guint16)rec->Data.MX.wPreference,
                        rec->Data.MX.pNameExchange);
}

static GVariant *
parse_dns_txt (DNS_RECORD *rec)
{
  GVariant *record;
  GPtrArray *array;
  DWORD i;

  array = g_ptr_array_new ();
  for (i = 0; i < rec->Data.TXT.dwStringCount; i++)
    g_ptr_array_add (array, rec->Data.TXT.pStringArray[i]);
  record = g_variant_new ("(@as)",
                          g_variant_new_strv ((const gchar **)array->pdata, array->len));
  g_ptr_array_free (array, TRUE);
  return record;
}

WORD
_g_resolver_record_type_to_dnstype (GResolverRecordType type)
{
  switch (type)
  {
    case G_RESOLVER_RECORD_SRV:
      return DNS_TYPE_SRV;
    case G_RESOLVER_RECORD_TXT:
      return DNS_TYPE_TEXT;
    case G_RESOLVER_RECORD_SOA:
      return DNS_TYPE_SOA;
    case G_RESOLVER_RECORD_NS:
      return DNS_TYPE_NS;
    case G_RESOLVER_RECORD_MX:
      return DNS_TYPE_MX;
  }
  g_return_val_if_reached (-1);
}

/* Private method to process a DnsQuery response into GVariants */
GList *
_g_resolver_records_from_DnsQuery (const gchar  *rrname,
                                   WORD          dnstype,
                                   DNS_STATUS    status,
                                   DNS_RECORD   *results,
                                   GError      **error)
{
  DNS_RECORD *rec;
  gpointer record;
  GList *records;

  if (status != ERROR_SUCCESS)
    {
      GResolverError errnum;
      const gchar *format;

      if (status == DNS_ERROR_RCODE_NAME_ERROR)
        {
          errnum = G_RESOLVER_ERROR_NOT_FOUND;
          format = _("No DNS record of the requested type for '%s'");
        }
      else if (status == DNS_ERROR_RCODE_SERVER_FAILURE)
        {
          errnum = G_RESOLVER_ERROR_TEMPORARY_FAILURE;
          format = _("Temporarily unable to resolve '%s'");
        }
      else
        {
          errnum = G_RESOLVER_ERROR_INTERNAL;
          format = _("Error resolving '%s'");
        }

      g_set_error (error, G_RESOLVER_ERROR, errnum, format, rrname);
      return NULL;
    }

  records = NULL;
  for (rec = results; rec; rec = rec->pNext)
    {
      if (rec->wType != dnstype)
        continue;
      switch (dnstype)
        {
        case DNS_TYPE_SRV:
          record = parse_dns_srv (rec);
          break;
        case DNS_TYPE_SOA:
          record = parse_dns_soa (rec);
          break;
        case DNS_TYPE_NS:
          record = parse_dns_ns (rec);
          break;
        case DNS_TYPE_MX:
          record = parse_dns_mx (rec);
          break;
        case DNS_TYPE_TEXT:
          record = parse_dns_txt (rec);
          break;
        default:
          g_warn_if_reached ();
          record = NULL;
          break;
        }
      if (record != NULL)
        records = g_list_prepend (records, g_variant_ref_sink (record));
    }

  return records;
}

#endif
