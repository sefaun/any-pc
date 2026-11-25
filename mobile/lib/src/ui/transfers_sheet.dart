import 'package:flutter/material.dart';
import 'package:share_plus/share_plus.dart';

import '../rtc/file_transfer.dart';

/// Bottom sheet listing file transfers with progress.
class TransfersSheet extends StatelessWidget {
  const TransfersSheet({super.key, required this.service, required this.onSend});

  final FileTransferService service;
  final Future<void> Function() onSend;

  @override
  Widget build(BuildContext context) {
    return ListenableBuilder(
      listenable: service,
      builder: (context, _) {
        final transfers = service.transfers.reversed.toList();
        return SafeArea(
          child: ConstrainedBox(
            constraints: BoxConstraints(maxHeight: MediaQuery.of(context).size.height * 0.7),
            child: Column(
              mainAxisSize: MainAxisSize.min,
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                Padding(
                  padding: const EdgeInsets.fromLTRB(16, 0, 8, 8),
                  child: Row(
                    children: [
                      Expanded(child: Text('File transfer', style: Theme.of(context).textTheme.titleMedium)),
                      FilledButton.tonalIcon(onPressed: onSend, icon: const Icon(Icons.upload_file), label: const Text('Send files')),
                    ],
                  ),
                ),
                if (transfers.isEmpty)
                  const Padding(
                    padding: EdgeInsets.all(24),
                    child: Text('No transfers yet. Files sent from the computer appear here.', textAlign: TextAlign.center),
                  )
                else
                  Flexible(
                    child: ListView.builder(
                      shrinkWrap: true,
                      itemCount: transfers.length,
                      itemBuilder: (context, i) => _TransferTile(transfer: transfers[i]),
                    ),
                  ),
              ],
            ),
          ),
        );
      },
    );
  }
}

class _TransferTile extends StatelessWidget {
  const _TransferTile({required this.transfer});
  final FileTransfer transfer;

  static String _size(int bytes) {
    if (bytes < 1024) return '$bytes B';
    if (bytes < 1024 * 1024) return '${(bytes / 1024).toStringAsFixed(1)} KB';
    return '${(bytes / (1024 * 1024)).toStringAsFixed(1)} MB';
  }

  @override
  Widget build(BuildContext context) {
    final outgoing = transfer.direction == TransferDirection.outgoing;
    final status = switch (transfer.state) {
      TransferState.queued => 'Waiting',
      TransferState.transferring => outgoing ? 'Sending' : 'Receiving',
      TransferState.completed => outgoing ? 'Sent' : 'Saved',
      TransferState.failed => 'Failed: ${transfer.error ?? ''}',
    };
    final canShare = !outgoing && transfer.state == TransferState.completed && transfer.path != null;
    return ListTile(
      leading: Icon(outgoing ? Icons.arrow_upward : Icons.arrow_downward),
      title: Text(transfer.name, maxLines: 1, overflow: TextOverflow.ellipsis),
      subtitle: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          const SizedBox(height: 4),
          LinearProgressIndicator(value: transfer.progress.clamp(0, 1)),
          const SizedBox(height: 4),
          Text('${_size(transfer.transferred)} / ${_size(transfer.size)} · $status'),
        ],
      ),
      trailing: canShare
          ? IconButton(
              icon: const Icon(Icons.ios_share),
              tooltip: 'Open / share',
              onPressed: () => SharePlus.instance.share(ShareParams(files: [XFile(transfer.path!)])),
            )
          : null,
    );
  }
}
