/// Normalises a user-entered server address into a base URL.
///
/// A missing scheme defaults to https; query, fragment and trailing slashes
/// are removed. Returns null for unusable input.
Uri? normalizeServerUrl(String input) {
  var text = input.trim();
  if (text.isEmpty) return null;
  if (!text.contains('://')) text = 'https://$text';
  final Uri uri;
  try {
    uri = Uri.parse(text);
  } on FormatException {
    return null;
  }
  if ((uri.scheme != 'http' && uri.scheme != 'https') || uri.host.isEmpty) return null;
  var path = uri.path;
  while (path.endsWith('/')) {
    path = path.substring(0, path.length - 1);
  }
  return Uri(scheme: uri.scheme, host: uri.host, port: uri.hasPort ? uri.port : null, path: path);
}

/// Appends [path] (starting with '/') to a base URL, preserving its base path.
Uri joinUrl(Uri base, String path) => base.replace(path: '${base.path}$path');
