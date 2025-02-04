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
import { ObjectID } from "bson";

import { createService } from "../services/MongoDBService";

import { MockFetcher } from "./utils";

/** A test interface that documents in my-collection implements */
interface MyDocument extends Realm.Services.MongoDB.Document {
    /** The name of the thing ... */
    name: string;
    /** Date the thing was created ... */
    createdAt: Date;
}

const DEFAULT_HEADERS = {
    Accept: "application/json",
    "Content-Type": "application/json",
};

describe("MongoDB Remote service", () => {
    it("can find documents", async () => {
        const fetcher = new MockFetcher([
            [
                {
                    _id: {
                        $oid: "deadbeefdeadbeefdeadbeef",
                    },
                    name: "Some document name ...",
                },
            ],
        ]);
        const service = createService(fetcher, "my-mongodb-service");
        const result = await service
            .db("my-database")
            .collection<MyDocument>("my-collection")
            .find(
                {
                    _id: ObjectID.createFromHexString(
                        "deadbeefdeadbeefdeadbeef",
                    ),
                },
                { limit: 10 },
            );
        // Expect the service to issue a request via the functions factory
        expect(fetcher.requests).deep.equals([
            {
                method: "POST",
                body: {
                    service: "my-mongodb-service",
                    name: "find",
                    arguments: [
                        {
                            database: "my-database",
                            collection: "my-collection",
                            limit: {
                                $numberInt: "10",
                            },
                            query: {
                                _id: { $oid: "deadbeefdeadbeefdeadbeef" },
                            },
                        },
                    ],
                },
                url: "http://localhost:1337/api/client/v2.0/app/mocked-app-id/functions/call",
                headers: DEFAULT_HEADERS,
            },
        ]);
        // TODO: Expect something about the findResult
        expect(typeof result).equals("object");
        const [firstDocument] = result;
        // Expect that the first document is EJSON deserialized
        expect(typeof firstDocument).equals("object");
        expect(typeof firstDocument._id).equals("object");
        expect(firstDocument._id.constructor.name).equals("ObjectId");
        expect(firstDocument.name).equals("Some document name ...");
    });

    it("can find one document", async () => {
        const fetcher = new MockFetcher([
            {
                _id: {
                    $oid: "deadbeefdeadbeefdeadbeef",
                },
                name: "Some document name ...",
            },
        ]);
        const service = createService(fetcher, "my-mongodb-service");
        const result = await service
            .db("my-database")
            .collection<MyDocument>("my-collection")
            .findOne(
                {
                    _id: ObjectID.createFromHexString(
                        "deadbeefdeadbeefdeadbeef",
                    ),
                },
                {
                    projection: { name: 1 },
                    sort: { name: 1 },
                },
            );
        // Expect the service to issue a request via the functions factory
        expect(fetcher.requests).deep.equals([
            {
                method: "POST",
                body: {
                    service: "my-mongodb-service",
                    name: "findOne",
                    arguments: [
                        {
                            database: "my-database",
                            collection: "my-collection",
                            sort: { name: { $numberInt: "1" } },
                            project: { name: { $numberInt: "1" } },
                            query: {
                                _id: { $oid: "deadbeefdeadbeefdeadbeef" },
                            },
                        },
                    ],
                },
                url: "http://localhost:1337/api/client/v2.0/app/mocked-app-id/functions/call",
                headers: DEFAULT_HEADERS,
            },
        ]);
        // TODO: Expect something about the findResult
        expect(typeof result).equals("object");
        // Expect that the first document is EJSON deserialized
        expect(typeof result).equals("object");
        if (result) {
            expect(typeof result._id).equals("object");
            expect(result._id.constructor.name).equals("ObjectId");
            expect(result.name).equals("Some document name ...");
        }
    });

    it("can insert a document", async () => {
        const now = new Date();
        const fetcher = new MockFetcher([
            {
                insertedId: { $oid: "deadbeefdeadbeefdeadbeef" },
            },
        ]);
        const service = createService(fetcher, "my-mongodb-service");
        const result = await service
            .db("my-database")
            .collection<MyDocument>("my-collection")
            .insertOne({
                name: "My awesome new document",
                createdAt: now,
            });
        expect(typeof result).equals("object");
        expect(typeof result.insertedId).equals("object");
        expect(result.insertedId.constructor.name).equals("ObjectId");

        expect(fetcher.requests).deep.equals([
            {
                method: "POST",
                body: {
                    service: "my-mongodb-service",
                    name: "insertOne",
                    arguments: [
                        {
                            database: "my-database",
                            collection: "my-collection",
                            document: {
                                name: "My awesome new document",
                                createdAt: {
                                    $date: {
                                        $numberLong: now.getTime().toString(),
                                    },
                                },
                            },
                        },
                    ],
                },
                url: "http://localhost:1337/api/client/v2.0/app/mocked-app-id/functions/call",
                headers: DEFAULT_HEADERS,
            },
        ]);
    });

    it("can insert many documents", async () => {
        const now = new Date();
        const fetcher = new MockFetcher([
            {
                insertedIds: [
                    { $oid: "deadbeefdeadbeefdead0001" },
                    { $oid: "deadbeefdeadbeefdead0002" },
                ],
            },
        ]);
        const service = createService(fetcher, "my-mongodb-service");
        const result = await service
            .db("my-database")
            .collection<MyDocument>("my-collection")
            .insertMany([
                {
                    name: "My first document",
                    createdAt: now,
                },
                {
                    name: "My second document",
                    createdAt: now,
                },
            ]);
        expect(typeof result).equals("object");
        expect(Array.isArray(result.insertedIds));
        for (const id of result.insertedIds) {
            expect(typeof id).equals("object");
            expect(id.constructor.name).equals("ObjectId");
        }

        expect(fetcher.requests).deep.equals([
            {
                method: "POST",
                body: {
                    service: "my-mongodb-service",
                    name: "insertMany",
                    arguments: [
                        {
                            database: "my-database",
                            collection: "my-collection",
                            documents: [
                                {
                                    name: "My first document",
                                    createdAt: {
                                        $date: {
                                            $numberLong: now
                                                .getTime()
                                                .toString(),
                                        },
                                    },
                                },
                                {
                                    name: "My second document",
                                    createdAt: {
                                        $date: {
                                            $numberLong: now
                                                .getTime()
                                                .toString(),
                                        },
                                    },
                                },
                            ],
                        },
                    ],
                },
                url: "http://localhost:1337/api/client/v2.0/app/mocked-app-id/functions/call",
                headers: DEFAULT_HEADERS,
            },
        ]);
    });

    it("can count documents", async () => {
        const fetcher = new MockFetcher([{ $numberLong: "1337" }]);
        const service = createService(fetcher, "my-mongodb-service");
        const result = await service
            .db("my-database")
            .collection<MyDocument>("my-collection")
            .count({}, { limit: 9999 });
        expect(result).equals(1337);
        expect(fetcher.requests).deep.equals([
            {
                method: "POST",
                body: {
                    service: "my-mongodb-service",
                    name: "count",
                    arguments: [
                        {
                            database: "my-database",
                            collection: "my-collection",
                            limit: { $numberInt: "9999" },
                            query: {},
                        },
                    ],
                },
                url: "http://localhost:1337/api/client/v2.0/app/mocked-app-id/functions/call",
                headers: DEFAULT_HEADERS,
            },
        ]);
    });
});
