INSERT OR IGNORE INTO country (iso, name, numcode) VALUES
('US', 'United States', 840),
('CA', 'Canada', 124),
('UK', 'United Kingdom', 826),
('DE', 'Germany', 276),
('FR', 'France', 250);

INSERT OR IGNORE INTO users (email, password, also_chef) VALUES
('john.doe@example.com', '$argon2i$v=19$m=65536,t=2,p=1$sDechuEZBtacwUkQX2w6Rg$YgTqHaLTbZUjrLd4P+uWSoKm7pFtf2pZaP1BwsxXefY', 0),
('jane.smith@example.com', '$argon2i$v=19$m=65536,t=2,p=1$sDechuEZBtacwUkQX2w6Rg$YgTqHaLTbZUjrLd4P+uWSoKm7pFtf2pZaP1BwsxXefY', 1),
('peter.jones@example.com', '$argon2i$v=19$m=65536,t=2,p=1$sDechuEZBtacwUkQX2w6Rg$YgTqHaLTbZUjrLd4P+uWSoKm7pFtf2pZaP1BwsxXefY', 0),
('alice.lee@example.com', '$argon2i$v=19$m=65536,t=2,p=1$sDechuEZBtacwUkQX2w6Rg$YgTqHaLTbZUjrLd4P+uWSoKm7pFtf2pZaP1BwsxXefY', 1),
('bob.williams@example.com', '$argon2i$v=19$m=65536,t=2,p=1$sDechuEZBtacwUkQX2w6Rg$YgTqHaLTbZUjrLd4P+uWSoKm7pFtf2pZaP1BwsxXefY', 0);

INSERT OR IGNORE INTO user_info (user_id, photo, name, surname, phone_number, country_id) VALUES
(1, 'https://images.pexels.com/photos/771742/pexels-photo-771742.jpeg?auto=compress&cs=tinysrgb&w=1260&h=750&dpr=2', 'John', 'Doe', '1234567890', 1),
(2, 'https://images.pexels.com/photos/771742/pexels-photo-771742.jpeg?auto=compress&cs=tinysrgb&w=1260&h=750&dpr=2', 'Jane', 'Smith', '9876543210', 2),
(3, 'https://images.pexels.com/photos/771742/pexels-photo-771742.jpeg?auto=compress&cs=tinysrgb&w=1260&h=750&dpr=2', 'Peter', 'Jones', '1112223333', 1),
(4, 'https://images.pexels.com/photos/771742/pexels-photo-771742.jpeg?auto=compress&cs=tinysrgb&w=1260&h=750&dpr=2', 'Alice', 'Lee', '4445556666', 3),
(5, 'https://images.pexels.com/photos/771742/pexels-photo-771742.jpeg?auto=compress&cs=tinysrgb&w=1260&h=750&dpr=2', 'Bob', 'Williams', '7778889999', 4);

INSERT OR IGNORE INTO chef_info (user_id, photo, phone_number, description) VALUES
(2, 'https://images.pexels.com/photos/771742/pexels-photo-771742.jpeg?auto=compress&cs=tinysrgb&w=1260&h=750&dpr=2', '987-654-3211', 'Experienced chef specializing in Italian cuisine.'),
(4, 'https://images.pexels.com/photos/771742/pexels-photo-771742.jpeg?auto=compress&cs=tinysrgb&w=1260&h=750&dpr=2', '444-555-6667', 'Passionate baker creating delicious pastries.');

INSERT OR IGNORE INTO addresses (user_id, address) VALUES
(1, '123 Main St, Anytown'),
(2, '456 Oak Ave, Anytown'),
(3, '789 Pine Ln, Anytown'),
(4, '101 Elm St, Anytown'),
(5, '222 Maple Dr, Anytown');

INSERT OR IGNORE INTO products (name, ingredients_list, photo, price, rating, chef_id) VALUES
('Spaghetti Carbonara', 'Pasta, eggs, bacon, cheese', 'https://images.pexels.com/photos/2641886/pexels-photo-2641886.jpeg', 12.99, 4.5, 2),
('Chocolate Cake', 'Flour, sugar, cocoa, eggs', 'https://images.pexels.com/photos/2641886/pexels-photo-2641886.jpeg', 15.99, 4.8, 4),
('Chicken Alfredo', 'Pasta, chicken, cream sauce', 'https://images.pexels.com/photos/2641886/pexels-photo-2641886.jpeg', 14.50, 4.2, 2),
('Croissant', 'Flour, butter, yeast', 'https://images.pexels.com/photos/2641886/pexels-photo-2641886.jpeg', 3.50, 4.9, 4),
('Pizza Margherita', 'Dough, tomato sauce, mozzarella', 'https://images.pexels.com/photos/2641886/pexels-photo-2641886.jpeg', 10.00, 4.0, 2);

INSERT OR IGNORE INTO cart (user_id) VALUES
(1),
(3),
(5);

INSERT OR IGNORE INTO cart_items (cart_id, product_id, product_quantity) VALUES
(1, 1, 1),
(1, 3, 2),
(2, 5, 1),
(3, 2, 1);

INSERT OR IGNORE INTO receipts (user_id) VALUES
(1),
(2);

INSERT OR IGNORE INTO receipt_items (receipt_id, product_id, product_quantity, product_purchased_price) VALUES
(1, 1, 1, 12.99),
(1, 3, 2, 14.50),
(2, 2, 1, 15.99);
