/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
// TODO: Need API spec
 *
 * You are granted a license to use, reproduce and create derivative works of
 * this document.
 */

[Global=(Worker,VideoWorker),
 Exposed=VideoWorker]
interface VideoWorkerGlobalScope : WorkerGlobalScope {
  [Throws]
  void postMessage(any message, optional sequence<any> transfer);

  attribute EventHandler onvideoprocess;
  attribute EventHandler onmessage;
  
};
