////////////////////////////////////////////////////////////////////////////
//
// Copyright 2020 Realm Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
////////////////////////////////////////////////////////////////////////////
import { expect } from "chai";
import { URL } from "url";

import { Credentials } from "..";
import { OAuth2Helper } from "../OAuth2Helper";
import { MemoryStorage } from "../storage";

describe("OAuth2Helper", () => {
    it("can initiate a flow", async () => {
        const windowsOpened: URL[] = [];
        const storage = new MemoryStorage();
        const helper = new OAuth2Helper(storage, urlString => {
            const url = new URL(urlString);
            windowsOpened.push(url);
            // Simulating another tab updating the storage
            setTimeout(() => {
                const state = url.searchParams.get("state");
                storage.set(
                    `oauth2:state(${state}):result`,
                    JSON.stringify({
                        appId: "default-app-id",
                        userAuth: "our-little-secret",
                    }),
                );
            }, 0);
            return null;
        });

        const credentials =
            Credentials.google<Realm.Credentials.OAuth2RedirectPayload>(
                "http://localhost:1337/callback",
            );
        expect(typeof credentials.payload.redirectUrl).equals("string");

        const state = helper.generateState();
        const redirectUrl = `https://some-external-service.com?state=${state}`;
        const result = await helper.openWindowAndWaitForRedirect(
            redirectUrl,
            state,
        );
        expect(windowsOpened.length).equals(1);
        const [url] = windowsOpened;
        expect(url.hostname).equals("some-external-service.com");
        expect(result.userAuth).equals("our-little-secret");
    });
});
