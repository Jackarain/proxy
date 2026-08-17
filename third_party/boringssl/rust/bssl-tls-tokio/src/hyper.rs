// Copyright 2026 The BoringSSL Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

//! Hyper support

use crate::TlsConnector;

use std::{
    error::Error,
    fmt::Debug,
    future::Future,
    pin::Pin,
    sync::Arc,
    task::{
        Context,
        Poll, //
    }, //
};

use hyper::http;
use tower::Service;

/// A connector for `hyper` using `bssl-tls`.
#[derive(Clone)]
pub struct HyperBsslConnector<Inner> {
    inner: Inner,
    connector: Arc<TlsConnector>,
}

impl<Inner: Debug> std::fmt::Debug for HyperBsslConnector<Inner> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("HyperBsslConnector")
            .field("inner", &self.inner)
            .finish()
    }
}

impl<Inner> HyperBsslConnector<Inner> {
    /// Construct a new `HyperBsslConnector`.
    pub fn new(inner: Inner, connector: TlsConnector) -> Self {
        Self {
            inner,
            connector: Arc::new(connector),
        }
    }
}

impl<Inner> Service<http::Uri> for HyperBsslConnector<Inner>
where
    Inner: Service<http::Uri>,
    Inner::Response: tokio::io::AsyncRead + tokio::io::AsyncWrite + Unpin + Send + Sync + 'static,
    Inner::Future: Send + 'static,
    Inner::Error: Into<Box<dyn Error + Send + Sync>>,
{
    type Response = crate::TlsStream<bssl_tls::connection::Client, Inner::Response>;
    type Error = Box<dyn Error + Send + Sync>;
    type Future = Pin<Box<dyn Future<Output = Result<Self::Response, Self::Error>> + Send>>;

    fn poll_ready(&mut self, cx: &mut Context<'_>) -> Poll<Result<(), Self::Error>> {
        self.inner.poll_ready(cx).map_err(Into::into)
    }

    fn call(&mut self, uri: http::Uri) -> Self::Future {
        let domain = uri
            .host()
            .unwrap_or("")
            .trim_start_matches('[')
            .trim_end_matches(']')
            .to_string();
        if domain.is_empty() {
            return Box::pin(std::future::ready(Err("empty domain".into())));
        }
        let fut = self.inner.call(uri);
        let connector = self.connector.clone();

        Box::pin(async move {
            let stream = fut.await.map_err(Into::into)?;
            Ok(connector.connect(&domain, stream).await?)
        })
    }
}
